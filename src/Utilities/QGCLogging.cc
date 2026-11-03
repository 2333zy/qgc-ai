/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGCLogging.h"
#include "AppSettings.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QGlobalStatic>
#include <QtCore/QStringListModel>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

#include <cstdio>

QGC_LOGGING_CATEGORY(QGCLoggingLog, "QGCLoggingLog")

Q_GLOBAL_STATIC(QGCLogging, _qgcLogging)

static QtMessageHandler defaultHandler = nullptr;

static void msgHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Anything logged from inside the handler would re-enter here and recurse until
    // the stack runs out. QGCLogging itself logs (log rotation, IO errors), so guard.
    static thread_local bool handling = false;
    if (handling) {
        return;
    }
    handling = true;

    // Filter via QLoggingCategory rules early
    if (QLoggingCategory(context.category).isDebugEnabled()) {
        // Format the message using Qt's pattern
        const QString message = qFormatLogMessage(type, context, msg);

        // The in-app log model requires the application object: QGCLogging owns a
        // QTimer and lives on the calling thread's event loop. Before the app exists
        // that loop has no dispatcher, so building the singleton here would bind it
        // to a thread that can never deliver queued emissions. Qt Quick internals are
        // also dropped from the model to keep it readable.
        if (qgcApp() && !QString(context.category).startsWith("qt.quick")) {
            QGCLogging::instance()->log(message);
        }

        // Keep the message visible outside the GUI. Qt installs no handler by default,
        // so defaultHandler is null and without this fallback every log line would be
        // swallowed by the in-app model - which hides unit test results and CI output.
        if (defaultHandler) {
            defaultHandler(type, context, msg);
        } else {
            fputs(qPrintable(message + QLatin1Char('\n')), stderr);
        }
    }

    handling = false;
}

QGCLogging *QGCLogging::instance()
{
    return _qgcLogging();
}

QGCLogging::QGCLogging(QObject *parent)
    : QStringListModel(parent)
{
    // qCDebug(QGCLoggingLog) << this;

    _flushTimer.setInterval(kFlushIntervalMSecs);
    _flushTimer.setSingleShot(false);
    (void) connect(&_flushTimer, &QTimer::timeout, this, &QGCLogging::_flushToDisk);

    // A timer can only be started on a thread that already has an event dispatcher.
    // QGCLogging may be constructed during startup before QApplication is up; in that
    // case QTimer::start() would fail with "Timers can only be used with threads
    // started with QThread" and the flush would never run. _threadsafeLog retries.
    if (QAbstractEventDispatcher::instance(QThread::currentThread())) {
        _flushTimer.start();
    }

    // Connect the emitLog signal to threadsafeLog slot
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    const Qt::ConnectionType conntype = Qt::QueuedConnection;
#else
    const Qt::ConnectionType conntype = Qt::AutoConnection;
#endif
    (void) connect(this, &QGCLogging::emitLog, this, &QGCLogging::_threadsafeLog, conntype);
}

void QGCLogging::installHandler()
{
    // Define the format for qDebug/qWarning/etc output
    qSetMessagePattern(QStringLiteral("%{time process} - %{type}: %{message} (%{category}:%{function}:%{line})"));

    // Install our custom handler
    defaultHandler = qInstallMessageHandler(msgHandler);
}

void QGCLogging::log(const QString &message)
{
    if (_ioError) {
        return;
    }

    // Only hand the message to the event loop when there is a loop to hand it to.
    // Emitting across threads queues the call, and queuing onto a thread without an
    // event dispatcher dereferences a null dispatcher and crashes (0xC0000005, which
    // is what made --unittest die inside QGCLogging::log).
    QThread *const receiverThread = thread();
    const bool canQueue = (QThread::currentThread() != receiverThread)
        && (QAbstractEventDispatcher::instance(receiverThread) != nullptr);

    if (canQueue) {
        emit emitLog(message);
    } else {
        _threadsafeLog(message);
    }
}

void QGCLogging::_threadsafeLog(const QString &message)
{
    // Retry the deferred flush start once a usable event loop is available.
    if (!_flushTimer.isActive() && QAbstractEventDispatcher::instance(QThread::currentThread())) {
        _flushTimer.start();
    }

    // Notify view of new row
    const int line = rowCount();
    beginInsertRows(QModelIndex(), line, line);
    (void) QStringListModel::insertRows(line, 1);
    (void) setData(index(line, 0), message, Qt::DisplayRole);
    endInsertRows();

    // Trim old entries to cap memory usage
    static constexpr const int kMaxLogRows = kMaxLogFileSize / 100;
    if (rowCount() > kMaxLogRows) {
        const int removeCount = rowCount() - kMaxLogRows;
        beginRemoveRows(QModelIndex(), 0, removeCount - 1);
        (void) removeRows(0, removeCount);
        endRemoveRows();
    }

    // Queue for disk flush
    _pendingDiskWrites.append(message);
}

void QGCLogging::_rotateLogs()
{
    // Close the current log
    _logFile.close();

    // Full path without extension
    const QString basePath = _logFile.fileName();    // e.g. "/path/QGCConsole.log"
    const QFileInfo fileInfo(basePath);
    const QString dir = fileInfo.absolutePath();
    const QString name = fileInfo.baseName();        // "QGCConsole"
    const QString ext = fileInfo.completeSuffix();   // "log"

    // Rotate existing backups: QGCConsole.4.log → QGCConsole.5.log, …
    for (int i = kMaxBackupFiles - 1; i >= 1; --i) {
        const QString from = QStringLiteral("%1/%2.%3.%4").arg(dir, name).arg(i).arg(ext);
        const QString to = QStringLiteral("%1/%2.%3.%4").arg(dir, name).arg(i+1).arg(ext);
        if (QFile::exists(to)) {
            (void) QFile::remove(to);
        }
        if (QFile::exists(from)) {
            (void) QFile::rename(from, to);
        }
    }

    // Move the just‐closed log to “.1”
    const QString firstBackup = QStringLiteral("%1/%2.1.%3").arg(dir, name, ext);
    if (QFile::exists(firstBackup)) {
        (void) QFile::remove(firstBackup);
    }
    (void) QFile::rename(basePath, firstBackup);

    // Re‑open a fresh log file
    _logFile.setFileName(basePath);
    if (!_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        _ioError = true;
        if (QGCApplication *const app = qgcApp()) {
            app->showAppMessage(tr("Unable to reopen log file %1: %2").arg(_logFile.fileName(), _logFile.errorString()));
        }
    }
}

void QGCLogging::_flushToDisk()
{
    if (_pendingDiskWrites.isEmpty() || _ioError) {
        return;
    }

    // Nothing to flush until the application object exists; lines stay queued.
    QGCApplication *const app = qgcApp();
    if (!app) {
        return;
    }

    // Ensure log output enabled and file open
    if (!_logFile.isOpen()) {
        if (!app->logOutput()) {
            _pendingDiskWrites.clear();
            return;
        }

        const QString saveDirPath = SettingsManager::instance()->appSettings()->crashSavePath();
        const QDir saveDir(saveDirPath);
        const QString saveFilePath = saveDir.absoluteFilePath("QGCConsole.log");

        _logFile.setFileName(saveFilePath);
        if (!_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            _ioError = true;
            app->showAppMessage(tr("Open console log output file failed %1 : %2").arg(_logFile.fileName(), _logFile.errorString()));
            return;
        }
    }

    // Check size before writing
    if (_logFile.size() >= kMaxLogFileSize) {
        _rotateLogs();
    }

    // Write all pending lines
    QTextStream out(&_logFile);
    for (const QString &line : std::as_const(_pendingDiskWrites)) {
        out << line << '\n';
        if (out.status() != QTextStream::Ok) {
            _ioError = true;
            qCWarning(QGCLoggingLog) << "Error writing to log file:" << _logFile.errorString();
            break;
        }
    }
    (void) _logFile.flush();
    _pendingDiskWrites.clear();
}

void QGCLogging::writeMessages(const QString &destFile)
{
    // Snapshot current logs on GUI thread
    const QStringList logs = stringList();

    // Run the file write in a separate thread
    (void) QtConcurrent::run([this, destFile, logs]() {
        emit writeStarted();
        bool success = false;
        QSaveFile file(destFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            for (const QString &line : logs) {
                out << line << '\n';
            }
            success = ((out.status() == QTextStream::Ok) && file.commit());
        } else {
            qCWarning(QGCLoggingLog) << "write failed:" << file.errorString();
        }
        emit writeFinished(success);
    });
}
