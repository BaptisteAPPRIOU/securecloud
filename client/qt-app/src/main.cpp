#include "MainWindow.h"
#include "ThemeManager.h"
#include <QApplication>
#include <QtGlobal>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>
#include <cstdio>

namespace {
QFile log_file;

void log_message_handler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
  Q_UNUSED(type);
  Q_UNUSED(context);

  if (log_file.isOpen()) {
    QTextStream stream(&log_file);
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << " " << message << "\n";
    stream.flush();
  }

  const QByteArray local = message.toLocal8Bit();
  std::fprintf(stderr, "%s\n", local.constData());
}

QString init_log_file() {
  QString log_path = qEnvironmentVariable("SECURECLOUD_CLIENT_LOG");
  if (log_path.isEmpty()) {
    log_path = QStringLiteral("logs/dev-run/qt-client.log");
  }

  const QFileInfo info(log_path);
  QDir dir = info.dir();
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  log_file.setFileName(log_path);
  if (!log_file.open(QIODevice::Append | QIODevice::Text)) {
    return QString();
  }
  return log_path;
}

QUrl resolve_api_base() {
  const QByteArray env = qgetenv("SECURECLOUD_API_BASE");
  if (!env.isEmpty()) {
    const QUrl url = QUrl(QString::fromUtf8(env));
    if (url.isValid()) {
      return url;
    }
  }
  return QUrl(QStringLiteral("https://localhost:8443"));
}
} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  const QString log_path = init_log_file();
  qInstallMessageHandler(log_message_handler);

  qInfo() << "SecureCloud client starting";
  qInfo() << "Client log file:" << (log_path.isEmpty() ? QStringLiteral("<stderr>") : log_path);
  const QByteArray api_raw = qgetenv("SECURECLOUD_API_BASE");
  const QByteArray allow_self_signed_raw = qgetenv("SECURECLOUD_DEV_ALLOW_SELF_SIGNED");
  const QByteArray tls_ca_file_raw = qgetenv("SECURECLOUD_TLS_CA_FILE");
  const QString api_display = api_raw.isEmpty() ? QStringLiteral("<default>") : QString::fromUtf8(api_raw);
  const QString allow_self_signed_display =
      allow_self_signed_raw.isEmpty() ? QStringLiteral("<default:false>") : QString::fromUtf8(allow_self_signed_raw);
  const QString tls_ca_file_display =
      tls_ca_file_raw.isEmpty() ? QStringLiteral("<auto>") : QString::fromUtf8(tls_ca_file_raw);
  const QUrl api_base = resolve_api_base();
  qInfo() << "Config: SECURECLOUD_API_BASE=" << api_display << "resolved=" << api_base.toString();
  qInfo() << "Config: SECURECLOUD_DEV_ALLOW_SELF_SIGNED=" << allow_self_signed_display;
  qInfo() << "Config: SECURECLOUD_TLS_CA_FILE=" << tls_ca_file_display;
  qInfo() << "Config: window_size=720x480";
  qInfo() << "Config: theme=light";

  ThemeManager theme;
  theme.applyLightD(app, false); // D par défaut

  MainWindow w(&theme);
  w.resize(720, 480);
  w.show();

  return app.exec();
}
