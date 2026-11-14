/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QStringList>
#include <QElapsedTimer>

#include "QGCLoggingCategory.h"

Q_DECLARE_LOGGING_CATEGORY(VisionLog)

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class VisionSettings;

/// Interface between QGC and a local vision recognition service.
///
/// On request it grabs decoded frames from the main video stream for a few seconds,
/// POSTs them as JSON to <serverUrl>/analyze and shows the returned result.
/// Protocol (UTF-8 JSON):
///   GET  /health  -> {"ok":true, "ready":bool, "name":"...", "api":1}
///   POST /analyze {"frames":["<base64 JPEG>", ...]}
///              -> {"ok":true, "label":"...", "count":4, "text":"...", "detail":"...",
///                  "image":"<base64 JPEG>", "boxes":[{"x","y","w","h","label","score"}]}
///              or {"ok":false, "error":"..."}
/// Optionally starts the service process with QGC and stops it on exit.
class VisionController : public QObject
{
    Q_OBJECT

public:
    explicit VisionController(QObject* parent = nullptr);
    ~VisionController() override;

    Q_PROPERTY(bool     busy            READ busy           NOTIFY stateChanged)
    Q_PROPERTY(QString  status          READ status         NOTIFY stateChanged)
    Q_PROPERTY(bool     resultVisible   READ resultVisible  NOTIFY stateChanged)
    Q_PROPERTY(bool     resultOk        READ resultOk       NOTIFY stateChanged)
    Q_PROPERTY(int      count           READ count          NOTIFY stateChanged)
    Q_PROPERTY(QString  label           READ label          NOTIFY stateChanged)
    Q_PROPERTY(QString  text            READ text           NOTIFY stateChanged)
    Q_PROPERTY(QString  detail          READ detail         NOTIFY stateChanged)
    Q_PROPERTY(QString  imageUrl        READ imageUrl       NOTIFY stateChanged)
    Q_PROPERTY(QString  error           READ error          NOTIFY stateChanged)
    Q_PROPERTY(QString  serverState     READ serverState    NOTIFY serverChanged)   ///< "ready", "loading", "starting", "offline"
    Q_PROPERTY(QString  serverMessage   READ serverMessage  NOTIFY serverChanged)
    Q_PROPERTY(QString  serverName      READ serverName     NOTIFY serverChanged)
    Q_PROPERTY(QString  effectiveCommand READ effectiveCommand NOTIFY serverChanged)

    Q_INVOKABLE void start          ();     ///< Grab frames and run one recognition
    Q_INVOKABLE void dismiss        ();     ///< Hide the result panel
    Q_INVOKABLE void checkServer    ();     ///< Refresh server state
    Q_INVOKABLE void restartServer  ();     ///< Stop the server we started (if any) and start it again

    bool    busy            () const { return _phase != Idle; }
    QString status          () const { return _status; }
    bool    resultVisible   () const { return _resultVisible; }
    bool    resultOk        () const { return _resultOk; }
    int     count           () const { return _count; }
    QString label           () const { return _label; }
    QString text            () const { return _text; }
    QString detail          () const { return _detail; }
    QString imageUrl        () const { return _imageUrl; }
    QString error           () const { return _error; }
    QString serverState     () const { return _serverState; }
    QString serverMessage   () const { return _serverMessage; }
    QString serverName      () const { return _serverName; }
    QString effectiveCommand() const;

signals:
    void stateChanged   ();
    void serverChanged  ();

private slots:
    void _grabTick      ();
    void _frameGrabbed  (QImage image);
    void _captureTimeout();
    void _healthTick    ();

private:
    enum Phase { Idle, Capturing, Analyzing };

    void    _connectReceiver    ();
    void    _send               ();
    void    _finish             (bool ok, const QString& error);
    void    _setServer          (const QString& state, const QString& message);
    void    _startServer        ();
    void    _stopServer         ();
    QString _baseUrl            () const;

    VisionSettings*         _settings       = nullptr;
    QNetworkAccessManager*  _net            = nullptr;
    QProcess*               _process        = nullptr;
    QObject*                _connectedReceiver = nullptr;
    QTimer                  _grabTimer;
    QTimer                  _captureDeadline;
    QTimer                  _healthTimer;
    QElapsedTimer           _serverStarted;
    bool                    _healthPending  = false;

    Phase       _phase          = Idle;
    int         _target         = 0;
    int         _requested      = 0;
    QStringList _frames;
    int         _resultSeq      = 0;

    QString     _status;
    bool        _resultVisible  = false;
    bool        _resultOk       = false;
    int         _count          = 0;
    QString     _label;
    QString     _text;
    QString     _detail;
    QString     _imageUrl;
    QString     _error;
    QString     _serverState    = QStringLiteral("offline");
    QString     _serverMessage;
    QString     _serverName;
};
