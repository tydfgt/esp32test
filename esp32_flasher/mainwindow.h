#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QProcess>
#include <QTimer>

class QComboBox;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QProgressBar;
class QLabel;
class QRadioButton;
class QGroupBox;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 串口
    void refreshPorts();
    // 文件选择
    void browseFile();
    // 烧录
    void startFlash();
    void onFlashFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onFlashOutput();
    void onFlashError();
    // 编译
    void startCompile();
    // 清理
    void cleanBuild();
    // 串口监视
    void toggleSerialMonitor();
    void onSerialReadyRead();
    void sendSerialData();
    void onSerialPortChanged(int idx);
    // 模式切换
    void onModeChanged();

private:
    void setupUI();
    void appendLog(const QString &text, const QColor &color = Qt::black);
    void setControlsEnabled(bool enabled);
    void executeCommand(const QString &program, const QStringList &args);
    QString arduinoCliPath() const;

    // --- UI 控件 ---
    QComboBox    *m_portCombo;
    QComboBox    *m_boardCombo;
    QComboBox    *m_baudCombo;
    QLineEdit    *m_fileEdit;
    QPushButton  *m_browseBtn;
    QPushButton  *m_flashBtn;
    QPushButton  *m_compileBtn;
    QPushButton  *m_cleanBtn;
    QPushButton  *m_refreshPortsBtn;
    QRadioButton *m_modeArduino;
    QRadioButton *m_modeBin;
    QLineEdit    *m_addrEdit;
    QPlainTextEdit *m_logView;
    QProgressBar *m_progressBar;
    QLabel       *m_statusLabel;
    QTabWidget   *m_tabWidget;

    // 串口监视相关
    QComboBox    *m_monBaudCombo;
    QLineEdit    *m_sendEdit;
    QPushButton  *m_sendBtn;
    QPushButton  *m_monToggleBtn;
    QPlainTextEdit *m_monView;
    QSerialPort  *m_serialPort;
    bool          m_monitoring;

    // 烧录进程
    QProcess     *m_process;
    bool          m_isFlashing;
    int           m_progressStep;
};

#endif // MAINWINDOW_H
