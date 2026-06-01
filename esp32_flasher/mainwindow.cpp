#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QRadioButton>
#include <QTabWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QApplication>
#include <QCheckBox>

// ============================================================
// 构造 & 析构
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serialPort(nullptr)
    , m_monitoring(false)
    , m_process(nullptr)
    , m_isFlashing(false)
    , m_progressStep(0)
{
    setupUI();
    refreshPorts();
    setControlsEnabled(true);
}

MainWindow::~MainWindow()
{
    if (m_monitoring && m_serialPort) {
        m_serialPort->close();
        delete m_serialPort;
    }
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

// ============================================================
// UI 构建
// ============================================================

void MainWindow::setupUI()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainVBox = new QVBoxLayout(central);
    mainVBox->setContentsMargins(12, 12, 12, 12);
    mainVBox->setSpacing(10);

    // ── 标题 ──
    auto *titleLabel = new QLabel(QStringLiteral("<h2>🔌 ESP32 烧录工具</h2>"));
    titleLabel->setAlignment(Qt::AlignCenter);
    mainVBox->addWidget(titleLabel);

    // ── 设置区 ──
    auto *settingsGroup = new QGroupBox(QStringLiteral("烧录设置"));
    auto *grid = new QGridLayout(settingsGroup);
    grid->setSpacing(8);

    // 串口选择
    grid->addWidget(new QLabel(QStringLiteral("串口:")), 0, 0);
    m_portCombo = new QComboBox();
    m_portCombo->setMinimumWidth(180);
    m_portCombo->setEditable(true);
    grid->addWidget(m_portCombo, 0, 1);
    m_refreshPortsBtn = new QPushButton(QStringLiteral("🔄 刷新"));
    m_refreshPortsBtn->setFixedWidth(90);
    connect(m_refreshPortsBtn, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    grid->addWidget(m_refreshPortsBtn, 0, 2);

    // 板卡选择
    grid->addWidget(new QLabel(QStringLiteral("板卡:")), 1, 0);
    m_boardCombo = new QComboBox();
    m_boardCombo->addItems({
        QStringLiteral("ESP32 Dev Module (esp32:esp32:esp32)"),
        QStringLiteral("ESP32-S3 Dev Module (esp32:esp32:esp32s3)"),
        QStringLiteral("ESP32-S2 Dev Module (esp32:esp32:esp32s2)"),
        QStringLiteral("ESP32-C3 Dev Module (esp32:esp32:esp32c3)"),
        QStringLiteral("ESP32-C6 Dev Module (esp32:esp32:esp32c6)"),
        QStringLiteral("ESP32-H2 Dev Module (esp32:esp32:esp32h2)"),
        QStringLiteral("ESP32-P4 Dev Module (esp32:esp32:esp32p4)"),
        QStringLiteral("NodeMCU-32S (esp32:esp32:nodemcu-32s)"),
        QStringLiteral("ESP32-CAM (esp32:esp32:esp32cam)"),
    });
    grid->addWidget(m_boardCombo, 1, 1, 1, 2);

    // 模式切换
    grid->addWidget(new QLabel(QStringLiteral("模式:")), 2, 0);
    auto *modeLayout = new QHBoxLayout();
    m_modeArduino = new QRadioButton(QStringLiteral("Arduino 项目 (.ino)"));
    m_modeBin     = new QRadioButton(QStringLiteral("直接烧录 .bin"));
    m_modeArduino->setChecked(true);
    modeLayout->addWidget(m_modeArduino);
    modeLayout->addWidget(m_modeBin);
    modeLayout->addStretch();
    connect(m_modeArduino, &QRadioButton::toggled, this, &MainWindow::onModeChanged);
    grid->addLayout(modeLayout, 2, 1, 1, 2);

    // 文件选择
    grid->addWidget(new QLabel(QStringLiteral("文件:")), 3, 0);
    m_fileEdit = new QLineEdit();
    m_fileEdit->setPlaceholderText(QStringLiteral("选择 .ino 项目文件或编译产物 .bin ..."));
    grid->addWidget(m_fileEdit, 3, 1);
    m_browseBtn = new QPushButton(QStringLiteral("📂 选择"));
    m_browseBtn->setFixedWidth(90);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::browseFile);
    grid->addWidget(m_browseBtn, 3, 2);

    // 烧录地址 (仅 bin 模式)
    grid->addWidget(new QLabel(QStringLiteral("地址:")), 4, 0);
    m_addrEdit = new QLineEdit(QStringLiteral("0x10000"));
    m_addrEdit->setFixedWidth(120);
    m_addrEdit->setEnabled(false);
    auto *addrLayout = new QHBoxLayout();
    addrLayout->addWidget(m_addrEdit);
    auto *addrHint = new QLabel(QStringLiteral("(仅 .bin 模式) bootloader=0x1000, app=0x10000, partitions=0x8000"));
    addrHint->setStyleSheet("color: gray; font-size: 11px;");
    addrLayout->addWidget(addrHint);
    addrLayout->addStretch();
    grid->addLayout(addrLayout, 4, 1, 1, 2);

    // 烧录波特率
    grid->addWidget(new QLabel(QStringLiteral("波特率:")), 5, 0);
    m_baudCombo = new QComboBox();
    m_baudCombo->addItems({"921600", "460800", "230400", "115200", "57600"});
    m_baudCombo->setCurrentText("921600");
    m_baudCombo->setFixedWidth(130);
    grid->addWidget(m_baudCombo, 5, 1);

    mainVBox->addWidget(settingsGroup);

    // ── 操作按钮 ──
    auto *btnLayout = new QHBoxLayout();
    m_compileBtn = new QPushButton(QStringLiteral("📋 编译"));
    m_compileBtn->setMinimumHeight(36);
    m_compileBtn->setStyleSheet("QPushButton { font-weight: bold; }");
    connect(m_compileBtn, &QPushButton::clicked, this, &MainWindow::startCompile);

    m_flashBtn = new QPushButton(QStringLiteral("🔥 烧录"));
    m_flashBtn->setMinimumHeight(36);
    m_flashBtn->setStyleSheet("QPushButton { font-weight: bold; background-color: #e74c3c; color: white; }");
    connect(m_flashBtn, &QPushButton::clicked, this, &MainWindow::startFlash);

    m_cleanBtn = new QPushButton(QStringLiteral("🗑 清理"));
    m_cleanBtn->setMinimumHeight(36);
    connect(m_cleanBtn, &QPushButton::clicked, this, &MainWindow::cleanBuild);

    btnLayout->addWidget(m_compileBtn);
    btnLayout->addWidget(m_flashBtn);
    btnLayout->addWidget(m_cleanBtn);
    btnLayout->addStretch();
    mainVBox->addLayout(btnLayout);

    // ── 进度 & 状态 ──
    auto *statusLayout = new QHBoxLayout();
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(20);
    statusLayout->addWidget(m_progressBar, 1);
    m_statusLabel = new QLabel(QStringLiteral("就绪"));
    m_statusLabel->setFixedWidth(160);
    statusLayout->addWidget(m_statusLabel);
    mainVBox->addLayout(statusLayout);

    // ── 日志 / 监视 Tab ──
    m_tabWidget = new QTabWidget();

    // 烧录日志 tab
    auto *logTab = new QWidget();
    auto *logLayout = new QVBoxLayout(logTab);
    logLayout->setContentsMargins(0, 0, 0, 0);
    m_logView = new QPlainTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont(QStringLiteral("Monospace"), 10));
    m_logView->setStyleSheet("QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    m_logView->setMaximumBlockCount(5000);
    logLayout->addWidget(m_logView);
    m_tabWidget->addTab(logTab, QStringLiteral("📜 烧录日志"));

    // 串口监视 tab
    auto *monTab = new QWidget();
    auto *monLayout = new QVBoxLayout(monTab);
    monLayout->setContentsMargins(4, 4, 4, 4);

    auto *monToolbar = new QHBoxLayout();
    m_monToggleBtn = new QPushButton(QStringLiteral("▶ 打开串口"));
    m_monToggleBtn->setFixedWidth(110);
    connect(m_monToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleSerialMonitor);
    monToolbar->addWidget(m_monToggleBtn);

    monToolbar->addWidget(new QLabel(QStringLiteral("波特率:")));
    m_monBaudCombo = new QComboBox();
    m_monBaudCombo->addItems({"115200", "921600", "460800", "230400", "57600", "9600"});
    m_monBaudCombo->setCurrentText("115200");
    m_monBaudCombo->setFixedWidth(100);
    monToolbar->addWidget(m_monBaudCombo);
    monToolbar->addStretch();
    monLayout->addLayout(monToolbar);

    m_monView = new QPlainTextEdit();
    m_monView->setReadOnly(true);
    m_monView->setFont(QFont(QStringLiteral("Monospace"), 10));
    m_monView->setStyleSheet("QPlainTextEdit { background-color: #1e1e1e; color: #00ff00; }");
    m_monView->setMaximumBlockCount(5000);
    monLayout->addWidget(m_monView);

    auto *sendLayout = new QHBoxLayout();
    m_sendEdit = new QLineEdit();
    m_sendEdit->setPlaceholderText(QStringLiteral("输入要发送的数据..."));
    connect(m_sendEdit, &QLineEdit::returnPressed, this, &MainWindow::sendSerialData);
    sendLayout->addWidget(m_sendEdit, 1);
    m_sendBtn = new QPushButton(QStringLiteral("发送"));
    m_sendBtn->setFixedWidth(80);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::sendSerialData);
    sendLayout->addWidget(m_sendBtn);
    auto *addCR = new QCheckBox(QStringLiteral("\\n"));
    addCR->setChecked(true);
    sendLayout->addWidget(addCR);
    // connect checkbox to send logic handled in sendSerialData via sender()
    monLayout->addLayout(sendLayout);

    m_tabWidget->addTab(monTab, QStringLiteral("📡 串口监视"));
    mainVBox->addWidget(m_tabWidget, 1);
}

// ============================================================
// 辅助函数
// ============================================================

QString MainWindow::arduinoCliPath() const
{
    return QStringLiteral("/home/cedar/.local/bin/arduino-cli");
}

void MainWindow::appendLog(const QString &text, const QColor &color)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString html = QStringLiteral("<span style='color:gray'>[%1]</span> "
                                   "<span style='color:%2'>%3</span>")
                       .arg(timestamp, color.name(), text.toHtmlEscaped());
    m_logView->appendHtml(html);
    // 自动滚到底部
    m_logView->moveCursor(QTextCursor::End);
}

void MainWindow::setControlsEnabled(bool enabled)
{
    m_portCombo->setEnabled(enabled);
    m_boardCombo->setEnabled(enabled);
    m_baudCombo->setEnabled(enabled);
    m_fileEdit->setEnabled(enabled);
    m_browseBtn->setEnabled(enabled);
    m_modeArduino->setEnabled(enabled);
    m_modeBin->setEnabled(enabled);
    m_addrEdit->setEnabled(enabled && m_modeBin->isChecked());
    m_flashBtn->setEnabled(enabled);
    m_compileBtn->setEnabled(enabled);
    m_cleanBtn->setEnabled(enabled);
    m_refreshPortsBtn->setEnabled(enabled);
    if (!enabled) {
        m_progressBar->setValue(0);
    }
}

// ============================================================
// 串口扫描
// ============================================================

void MainWindow::refreshPorts()
{
    QString current = m_portCombo->currentText();
    m_portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &p : ports) {
        QString sysName = "/dev/" + p.portName();  // Linux: QSerialPortInfo::portName() 只返回 "ttyUSB0"
        QString desc = QStringLiteral("%1 - %2")
                           .arg(sysName, p.description());
        if (!p.manufacturer().isEmpty())
            desc += QStringLiteral(" [%1]").arg(p.manufacturer());
        m_portCombo->addItem(desc, sysName);
    }
    // 尝试恢复之前的选择
    int idx = m_portCombo->findText(current, Qt::MatchContains);
    if (idx >= 0) m_portCombo->setCurrentIndex(idx);
    else if (!current.isEmpty()) m_portCombo->setCurrentText(current);
    // 默认 USB0
    if (m_portCombo->count() == 0) {
        m_portCombo->addItem("/dev/ttyUSB0", "/dev/ttyUSB0");
    }

    appendLog(QStringLiteral("已扫描到 %1 个串口").arg(ports.size()), QColor("#569cd6"));
}

// ============================================================
// 文件浏览
// ============================================================

void MainWindow::browseFile()
{
    QString path;
    if (m_modeArduino->isChecked()) {
        path = QFileDialog::getOpenFileName(this,
            QStringLiteral("选择 Arduino 项目文件"),
            QStringLiteral("/home/cedar/esp32/projects"),
            QStringLiteral("Arduino 项目 (*.ino);;所有文件 (*)"));
    } else {
        path = QFileDialog::getOpenFileName(this,
            QStringLiteral("选择固件 .bin 文件"),
            QStringLiteral("/home/cedar/esp32/projects"),
            QStringLiteral("BIN 文件 (*.bin);;所有文件 (*)"));
    }
    if (!path.isEmpty()) {
        m_fileEdit->setText(path);
    }
}

// ============================================================
// 模式切换
// ============================================================

void MainWindow::onModeChanged()
{
    bool isBin = m_modeBin->isChecked();
    m_addrEdit->setEnabled(isBin && !m_isFlashing);
    m_boardCombo->setEnabled(!isBin || !m_isFlashing);
    m_compileBtn->setVisible(!isBin);
    if (isBin) {
        m_fileEdit->setPlaceholderText(QStringLiteral("选择 .bin 固件文件..."));
    } else {
        m_fileEdit->setPlaceholderText(QStringLiteral("选择 .ino 项目文件..."));
    }
}

// ============================================================
// 执行外部命令
// ============================================================

void MainWindow::executeCommand(const QString &program, const QStringList &args)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        appendLog(QStringLiteral("⚠ 上一个任务仍在运行中"), QColor("#e5c07b"));
        return;
    }

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &MainWindow::onFlashOutput);
        connect(m_process, &QProcess::readyReadStandardError, this, &MainWindow::onFlashError);
        connect(m_process, &QProcess::finished,
                this, &MainWindow::onFlashFinished);
    }

    m_isFlashing = true;
    m_progressStep = 0;
    m_progressBar->setValue(0);
    setControlsEnabled(false);
    m_logView->clear();

    appendLog(QStringLiteral("══════════════════════════════"), QColor("#888"));
    appendLog(QStringLiteral("命令: %1 %2").arg(program, args.join(' ')), QColor("#569cd6"));
    appendLog(QStringLiteral("══════════════════════════════"), QColor("#888"));

    // 设置环境变量
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", "/home/cedar/.local/bin:" + env.value("PATH"));
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(QFileInfo(m_fileEdit->text()).absolutePath());

    m_statusLabel->setText(QStringLiteral("运行中..."));
    m_process->start(program, args);
}

// ============================================================
// 编译
// ============================================================

void MainWindow::startCompile()
{
    QString inoFile = m_fileEdit->text().trimmed();
    if (inoFile.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择 .ino 项目文件"));
        return;
    }
    if (!QFileInfo::exists(arduinoCliPath())) {
        QMessageBox::critical(this, QStringLiteral("错误"),
            QStringLiteral("找不到 arduino-cli: %1").arg(arduinoCliPath()));
        return;
    }

    // 提取 FQBN
    QString boardText = m_boardCombo->currentText();
    QRegularExpression re(R"(\(([^)]+)\))");
    auto match = re.match(boardText);
    QString fqbn = match.hasMatch() ? match.captured(1) : "esp32:esp32:esp32";

    QStringList args;
    args << "compile" << "--fqbn" << fqbn << inoFile;

    // 如果项目有 build 目录设置
    QString buildDir = QFileInfo(inoFile).absolutePath() + "/build";
    if (QDir(buildDir).exists()) {
        args << "--output-dir" << buildDir;
    }

    executeCommand(arduinoCliPath(), args);
}

// ============================================================
// 烧录
// ============================================================

void MainWindow::startFlash()
{
    QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty()) {
        portName = m_portCombo->currentText().split(' ').first();
    }
    // 确保端口名以 /dev/ 开头
    if (!portName.startsWith('/'))
        portName = "/dev/" + portName;

    if (m_modeArduino->isChecked()) {
        // ── Arduino 模式: compile + upload ──
        QString inoFile = m_fileEdit->text().trimmed();
        if (inoFile.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择 .ino 项目文件"));
            return;
        }
        if (!QFileInfo::exists(arduinoCliPath())) {
            QMessageBox::critical(this, QStringLiteral("错误"),
                QStringLiteral("找不到 arduino-cli: %1").arg(arduinoCliPath()));
            return;
        }

        QString boardText = m_boardCombo->currentText();
        QRegularExpression re(R"(\(([^)]+)\))");
        auto match = re.match(boardText);
        QString fqbn = match.hasMatch() ? match.captured(1) : "esp32:esp32:esp32";

        QStringList args;
        args << "compile" << "--fqbn" << fqbn
             << "-u" << "-p" << portName
             << inoFile;

        QString buildDir = QFileInfo(inoFile).absolutePath() + "/build";
        if (QDir(buildDir).exists()) {
            args << "--output-dir" << buildDir;
        }

        executeCommand(arduinoCliPath(), args);
    } else {
        // ── Bin 模式: esptool 直接烧录 ──
        QString binFile = m_fileEdit->text().trimmed();
        if (binFile.isEmpty() || !QFileInfo::exists(binFile)) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择有效的 .bin 文件"));
            return;
        }

        QString addr = m_addrEdit->text().trimmed();
        if (addr.isEmpty()) addr = "0x10000";

        QString esptoolPath = QStringLiteral("/home/cedar/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool");
        QString baud = m_baudCombo->currentText();

        QStringList args;
        args << "-u" << esptoolPath
             << "--chip" << "esp32"
             << "--port" << portName
             << "--baud" << baud
             << "write_flash"
             << addr << binFile;

        executeCommand(QStringLiteral("/usr/bin/python3"), args);
    }

    m_statusLabel->setText(QStringLiteral("烧录中..."));
}

// ============================================================
// 进程输出 & 完成
// ============================================================

void MainWindow::onFlashOutput()
{
    QString data = QString::fromUtf8(m_process->readAllStandardOutput());
    m_logView->appendPlainText(data);
    m_logView->moveCursor(QTextCursor::End);

    // 简易进度推断
    if (data.contains("Compiling") || data.contains("编译")) {
        m_progressBar->setValue(qMin(30, m_progressBar->value() + 2));
    } else if (data.contains("Linking") || data.contains("链接")) {
        m_progressBar->setValue(60);
    } else if (data.contains("Upload") || data.contains("Writing") || data.contains("烧录")) {
        m_progressBar->setValue(qMin(95, m_progressBar->value() + 1));
    }
}

void MainWindow::onFlashError()
{
    QString data = QString::fromUtf8(m_process->readAllStandardError());
    m_logView->appendPlainText(data);
    m_logView->moveCursor(QTextCursor::End);
}

void MainWindow::onFlashFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_isFlashing = false;
    setControlsEnabled(true);

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        m_progressBar->setValue(0);
        m_statusLabel->setText(QStringLiteral("❌ 失败 (exit %1)").arg(exitCode));
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
        appendLog(QStringLiteral("\n❌ 操作失败，退出码: %1").arg(exitCode), QColor("#e74c3c"));
        // 切到日志 tab 查看错误
        m_tabWidget->setCurrentIndex(0);
    } else {
        m_progressBar->setValue(100);
        m_statusLabel->setText(QStringLiteral("✅ 完成"));
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
        appendLog(QStringLiteral("\n✅ 操作成功完成！"), QColor("#00ff00"));
    }

    appendLog(QStringLiteral("══════════════════════════════\n"), QColor("#888"));
}

// ============================================================
// 清理
// ============================================================

void MainWindow::cleanBuild()
{
    if (m_isFlashing) return;

    QString inoFile = m_fileEdit->text().trimmed();
    QString buildDir;

    if (!inoFile.isEmpty() && m_modeArduino->isChecked()) {
        buildDir = QFileInfo(inoFile).absolutePath() + "/build";
    }

    if (!buildDir.isEmpty() && QDir(buildDir).exists()) {
        QDir(buildDir).removeRecursively();
        appendLog(QStringLiteral("🗑 已清理: %1").arg(buildDir), QColor("#e5c07b"));
        m_statusLabel->setText(QStringLiteral("已清理"));
    } else {
        // 用 arduino-cli --clean
        if (!inoFile.isEmpty() && QFileInfo::exists(arduinoCliPath())) {
            QString boardText = m_boardCombo->currentText();
            QRegularExpression re(R"(\(([^)]+)\))");
            auto match = re.match(boardText);
            QString fqbn = match.hasMatch() ? match.captured(1) : "esp32:esp32:esp32";
            QStringList args;
            args << "compile" << "--fqbn" << fqbn << "--clean" << inoFile;
            executeCommand(arduinoCliPath(), args);
            return;
        }
    }

    // 也清理 arduino 缓存
    QString cacheDir = QDir::homePath() + "/.cache/arduino";
    if (QDir(cacheDir).exists()) {
        QDir(cacheDir).removeRecursively();
        appendLog(QStringLiteral("🗑 已清理 Arduino 缓存"), QColor("#e5c07b"));
    }
}

// ============================================================
// 串口监视
// ============================================================

void MainWindow::toggleSerialMonitor()
{
    if (m_monitoring) {
        // 关闭
        if (m_serialPort) {
            m_serialPort->close();
            delete m_serialPort;
            m_serialPort = nullptr;
        }
        m_monitoring = false;
        m_monToggleBtn->setText(QStringLiteral("▶ 打开串口"));
        m_monToggleBtn->setStyleSheet("");
        m_monBaudCombo->setEnabled(true);
        m_portCombo->setEnabled(true);
        appendLog(QStringLiteral("📡 串口监视已关闭"), QColor("#569cd6"));
    } else {
        // 打开
        QString portName = m_portCombo->currentData().toString();
        if (portName.isEmpty()) {
            portName = m_portCombo->currentText().split(' ').first();
        }
        // 确保端口名以 /dev/ 开头
        if (!portName.startsWith('/'))
            portName = "/dev/" + portName;

        m_serialPort = new QSerialPort(this);
        m_serialPort->setPortName(portName);
        m_serialPort->setBaudRate(m_monBaudCombo->currentText().toInt());
        m_serialPort->setDataBits(QSerialPort::Data8);
        m_serialPort->setParity(QSerialPort::NoParity);
        m_serialPort->setStopBits(QSerialPort::OneStop);
        m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

        if (!m_serialPort->open(QIODevice::ReadWrite)) {
            appendLog(QStringLiteral("❌ 无法打开串口 %1: %2")
                          .arg(portName, m_serialPort->errorString()),
                      QColor("#e74c3c"));
            delete m_serialPort;
            m_serialPort = nullptr;
            return;
        }

        connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::onSerialReadyRead);

        m_monitoring = true;
        m_monToggleBtn->setText(QStringLiteral("⏹ 关闭串口"));
        m_monToggleBtn->setStyleSheet("QPushButton { background-color: #c0392b; color: white; }");
        m_monBaudCombo->setEnabled(false);
        m_portCombo->setEnabled(false);  // 占用时不能切换

        appendLog(QStringLiteral("📡 串口监视已开启: %1 @ %2 baud")
                      .arg(portName, m_monBaudCombo->currentText()),
                  QColor("#00ff00"));
        m_tabWidget->setCurrentIndex(1);
    }
}

void MainWindow::onSerialReadyRead()
{
    if (!m_serialPort) return;
    QByteArray data = m_serialPort->readAll();
    m_monView->insertPlainText(QString::fromUtf8(data));
    m_monView->moveCursor(QTextCursor::End);
}

void MainWindow::sendSerialData()
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先打开串口"));
        return;
    }

    QString text = m_sendEdit->text();
    // 检查是否有 \n checkbox (通过父布局找到)
    bool addNewline = true; // 默认加换行
    // 遍历找到 checkbox
    auto *sendLayout = m_sendEdit->parentWidget()->layout();
    if (sendLayout) {
        for (int i = 0; i < sendLayout->count(); i++) {
            auto *item = sendLayout->itemAt(i);
            if (item && item->widget()) {
                auto *cb = qobject_cast<QCheckBox*>(item->widget());
                if (cb) {
                    addNewline = cb->isChecked();
                    break;
                }
            }
        }
    }

    if (addNewline) text += '\n';
    QByteArray data = text.toUtf8();
    m_serialPort->write(data);

    // 回显
    m_monView->insertPlainText(QStringLiteral("> ") + text);
    m_monView->moveCursor(QTextCursor::End);
    m_sendEdit->clear();
}

void MainWindow::onSerialPortChanged(int /*idx*/)
{
    // 如果正在监视，不允许切换
    if (m_monitoring) {
        m_portCombo->setEnabled(false);
    }
}
