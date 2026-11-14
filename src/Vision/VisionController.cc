/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VisionController.h"
#include "QGCApplication.h"
#include "QGCToolbox.h"
#include "SettingsManager.h"
#include "VisionSettings.h"
#include "VideoManager.h"
#include "VideoReceiver.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

QGC_LOGGING_CATEGORY(VisionLog, "VisionLog")

static QString visionTempDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/qgc-vision");
    QDir().mkpath(dir);
    return dir;
}

VisionController::VisionController(QObject* parent)
    : QObject(parent)
{
    _settings = qgcApp()->toolbox()->settingsManager()->visionSettings();
    _net = new QNetworkAccessManager(this);

    connect(&_grabTimer,        &QTimer::timeout, this, &VisionController::_grabTick);
    connect(&_captureDeadline,  &QTimer::timeout, this, &VisionController::_captureTimeout);
    connect(&_healthTimer,      &QTimer::timeout, this, &VisionController::_healthTick);
    _captureDeadline.setSingleShot(true);

    _setServer(QStringLiteral("offline"), tr("未连接"));

    // First check whether a service is already running; start one if not.
    QNetworkRequest req(QUrl(_baseUrl() + QStringLiteral("/health")));
    req.setTransferTimeout(1500);
    QNetworkReply* reply = _net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError && _settings->autoStartServer()->rawValue().toBool()) {
            _startServer();
        }
        _healthTick();
        _healthTimer.start(2000);
    });

    // Test hook: QGC_VISION_AUTOTEST=1 runs one recognition as soon as video and service are ready,
    // and writes the result to <temp>/qgc-vision/autotest.json
    if (qEnvironmentVariableIsSet("QGC_VISION_AUTOTEST")) {
        QTimer* t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this, t]() {
            VideoManager* vm = qgcApp()->toolbox()->videoManager();
            if (_phase == Idle && _serverState == QStringLiteral("ready") && vm && vm->decoding()) {
                t->stop();
                connect(this, &VisionController::stateChanged, this, [this]() {
                    if (_phase != Idle) {
                        return;
                    }
                    QJsonObject o;
                    o.insert(QStringLiteral("ok"), _resultOk);
                    o.insert(QStringLiteral("count"), _count);
                    o.insert(QStringLiteral("text"), _text);
                    o.insert(QStringLiteral("detail"), _detail);
                    o.insert(QStringLiteral("error"), _error);
                    o.insert(QStringLiteral("image"), _imageUrl);
                    QFile f(visionTempDir() + QStringLiteral("/autotest.json"));
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(QJsonDocument(o).toJson());
                    }
                });
                start();
            }
        });
        t->start(1000);
    }
}

VisionController::~VisionController()
{
    _stopServer();
}

QString VisionController::_baseUrl() const
{
    QString url = _settings->serverUrl()->rawValue().toString().trimmed();
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    return url;
}

QString VisionController::effectiveCommand() const
{
    QString cmd = _settings->serverCommand()->rawValue().toString().trimmed();
#ifdef Q_OS_WIN
    if (cmd.isEmpty()) {
        // Registered by the model installer (crab-counter install.bat)
        QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\QGC-Vision"), QSettings::NativeFormat);
        cmd = reg.value(QStringLiteral("ServerCommand")).toString().trimmed();
    }
#endif
    return cmd;
}

// ---------------------------------------------------------------- server process

void VisionController::_startServer()
{
    if (_process && _process->state() != QProcess::NotRunning) {
        return;
    }
    const QString cmd = effectiveCommand();
    if (cmd.isEmpty()) {
        _setServer(QStringLiteral("offline"), tr("没有设置启动命令"));
        return;
    }
    QStringList args = QProcess::splitCommand(cmd);
    if (args.isEmpty()) {
        return;
    }
    const QString program = args.takeFirst();

    if (!_process) {
        _process = new QProcess(this);
        _process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
        _process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a) {
            a->flags |= 0x08000000; // CREATE_NO_WINDOW
        });
#endif
        connect(_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            qCWarning(VisionLog) << "Vision server process error:" << _process->errorString();
            _setServer(QStringLiteral("offline"), tr("启动失败：%1").arg(_process->errorString()));
        });
        connect(_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code, QProcess::ExitStatus) {
            qCWarning(VisionLog) << "Vision server exited with code" << code;
            _setServer(QStringLiteral("offline"), tr("服务已退出（代码 %1），日志：%2").arg(code).arg(QDir::toNativeSeparators(visionTempDir() + QStringLiteral("/server.log"))));
        });
    }
    _process->setStandardOutputFile(visionTempDir() + QStringLiteral("/server.log"), QIODevice::Truncate);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    _process->setProcessEnvironment(env);
    qCDebug(VisionLog) << "Starting vision server:" << program << args;
    _process->start(program, args);
    _serverStarted.start();
    _setServer(QStringLiteral("starting"), tr("正在启动服务…"));
}

void VisionController::_stopServer()
{
    if (_process && _process->state() != QProcess::NotRunning) {
        _process->disconnect(this);
        _process->closeWriteChannel();   // server started with --exit-with-parent exits on stdin EOF
        if (!_process->waitForFinished(2000)) {
            _process->kill();
            _process->waitForFinished(1000);
        }
    }
}

void VisionController::restartServer()
{
    _stopServer();
    if (_process) {
        _process->deleteLater();
        _process = nullptr;
    }
    _startServer();
    _healthTick();
}

void VisionController::checkServer()
{
    _healthTick();
}

void VisionController::_setServer(const QString& state, const QString& message)
{
    if (state != _serverState || message != _serverMessage) {
        _serverState = state;
        _serverMessage = message;
        emit serverChanged();
    }
}

void VisionController::_healthTick()
{
    if (_healthPending) {
        return;
    }
    _healthPending = true;
    QNetworkRequest req(QUrl(_baseUrl() + QStringLiteral("/health")));
    req.setTransferTimeout(1500);
    QNetworkReply* reply = _net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        _healthPending = false;
        if (reply->error() != QNetworkReply::NoError) {
            const bool starting = _process && _process->state() != QProcess::NotRunning && _serverStarted.isValid() && _serverStarted.elapsed() < 120000;
            if (starting) {
                _setServer(QStringLiteral("starting"), tr("正在启动服务…"));
            } else if (_serverState != QStringLiteral("offline") || _serverMessage.isEmpty() || _serverMessage == tr("正在启动服务…")) {
                _setServer(QStringLiteral("offline"), tr("连不上 %1").arg(_baseUrl()));
            }
            _healthTimer.setInterval(2000);
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString name = o.value(QStringLiteral("name")).toString();
        if (name != _serverName) {
            _serverName = name;
            emit serverChanged();
        }
        const QString err = o.value(QStringLiteral("error")).toString();
        if (o.value(QStringLiteral("ready")).toBool()) {
            _setServer(QStringLiteral("ready"), tr("已就绪"));
            _healthTimer.setInterval(10000);
        } else {
            _setServer(QStringLiteral("loading"), err.isEmpty() ? tr("模型加载中…") : err);
            _healthTimer.setInterval(2000);
        }
    });
}

// ---------------------------------------------------------------- recognition

void VisionController::_connectReceiver()
{
    VideoReceiver* receiver = qgcApp()->toolbox()->videoManager()->videoReceiver();
    if (receiver && receiver != _connectedReceiver) {
        connect(receiver, &VideoReceiver::frameGrabbed, this, &VisionController::_frameGrabbed, Qt::QueuedConnection);
        _connectedReceiver = receiver;
    }
}

void VisionController::start()
{
    if (_phase != Idle) {
        return;
    }
    VideoManager* vm = qgcApp()->toolbox()->videoManager();
    _resultVisible = true;
    _error.clear();
    _text.clear();
    _detail.clear();
    _label.clear();
    _count = 0;
    if (!vm || !vm->videoReceiver() || !vm->decoding()) {
        _finish(false, tr("没有视频画面，先确认视频在播放"));
        return;
    }
    _connectReceiver();

    const double seconds = _settings->captureSeconds()->rawValue().toDouble();
    const double fps     = _settings->captureFps()->rawValue().toDouble();
    _target     = qBound(1, qRound(seconds * fps), 150);
    _requested  = 0;
    _frames.clear();
    _phase      = Capturing;
    _status     = tr("取画面 %1/%2").arg(0).arg(_target);
    emit stateChanged();

    _grabTimer.start(qMax(30, qRound(1000.0 / fps)));
    _captureDeadline.start(qRound((seconds + 3.0) * 1000.0));
    _grabTick();
}

void VisionController::_grabTick()
{
    if (_phase != Capturing || _requested >= _target) {
        _grabTimer.stop();
        return;
    }
    VideoReceiver* receiver = qgcApp()->toolbox()->videoManager()->videoReceiver();
    if (receiver) {
        receiver->grabFrame();
        _requested++;
    }
}

void VisionController::_frameGrabbed(QImage image)
{
    if (_phase != Capturing || image.isNull()) {
        return;
    }
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPG", 90)) {
        qCWarning(VisionLog) << "JPEG encode failed";
        return;
    }
    _frames.append(QString::fromLatin1(jpeg.toBase64()));
    _status = tr("取画面 %1/%2").arg(_frames.count()).arg(_target);
    emit stateChanged();
    if (_frames.count() >= _target) {
        _send();
    }
}

void VisionController::_captureTimeout()
{
    if (_phase != Capturing) {
        return;
    }
    if (_frames.isEmpty()) {
        _finish(false, tr("取不到视频画面"));
    } else {
        _send();
    }
}

void VisionController::_send()
{
    _grabTimer.stop();
    _captureDeadline.stop();
    _phase  = Analyzing;
    _status = tr("识别中（%1 帧）…").arg(_frames.count());
    emit stateChanged();

    QJsonObject body;
    body.insert(QStringLiteral("frames"), QJsonArray::fromStringList(_frames));
    _frames.clear();

    QNetworkRequest req(QUrl(_baseUrl() + QStringLiteral("/analyze")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(180000);
    QNetworkReply* reply = _net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError && reply->error() < QNetworkReply::ContentAccessDenied) {
            _finish(false, tr("连不上识别服务 %1（%2）").arg(_baseUrl(), reply->errorString()));
            _healthTick();
            return;
        }
        QJsonParseError perr;
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll(), &perr).object();
        if (perr.error != QJsonParseError::NoError) {
            _finish(false, tr("识别服务返回的内容看不懂：%1").arg(reply->errorString()));
            return;
        }
        if (!o.value(QStringLiteral("ok")).toBool()) {
            _finish(false, o.value(QStringLiteral("error")).toString(tr("识别失败")));
            return;
        }
        _count  = o.value(QStringLiteral("count")).toInt();
        _label  = o.value(QStringLiteral("label")).toString();
        _text   = o.value(QStringLiteral("text")).toString();
        _detail = o.value(QStringLiteral("detail")).toString();

        const QByteArray img = QByteArray::fromBase64(o.value(QStringLiteral("image")).toString().toLatin1());
        if (!_imageUrl.isEmpty()) {
            QFile::remove(QUrl(_imageUrl).toLocalFile());
            _imageUrl.clear();
        }
        if (!img.isEmpty()) {
            const QString path = visionTempDir() + QStringLiteral("/result_%1.jpg").arg(++_resultSeq);
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(img);
                f.close();
                _imageUrl = QUrl::fromLocalFile(path).toString();
            }
        }
        _finish(true, QString());
    });
}

void VisionController::_finish(bool ok, const QString& error)
{
    _grabTimer.stop();
    _captureDeadline.stop();
    _frames.clear();
    _phase          = Idle;
    _resultOk       = ok;
    _error          = error;
    _status.clear();
    _resultVisible  = true;
    if (!ok) {
        qCWarning(VisionLog) << "Recognition failed:" << error;
    }
    emit stateChanged();
}

void VisionController::dismiss()
{
    if (_phase == Idle) {
        _resultVisible = false;
        emit stateChanged();
    }
}
