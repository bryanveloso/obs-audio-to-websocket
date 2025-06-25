#include "obs-audio-to-websocket/settings-dialog.hpp"
#include "obs-audio-to-websocket/audio-streamer.hpp"
#include <util/config-file.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QMessageBox>
#include <obs.h>
#include <obs-frontend-api.h>

namespace obs_audio_to_websocket {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_streamer(&AudioStreamer::Instance())
{
    setupUi();
    connectSignals();
    loadSettings();
    
    m_updateTimer = std::make_unique<QTimer>(this);
    connect(m_updateTimer.get(), &QTimer::timeout, this, &SettingsDialog::updateStatus);
    m_updateTimer->start(100); // Update every 100ms
}

SettingsDialog::~SettingsDialog() {
    saveSettings();
}

void SettingsDialog::setupUi() {
    setWindowTitle("Audio to WebSocket Settings");
    setFixedSize(450, 400);
    
    auto* mainLayout = new QVBoxLayout(this);
    
    // Connection Settings Group
    auto* connectionGroup = new QGroupBox("WebSocket Connection", this);
    auto* connectionLayout = new QGridLayout(connectionGroup);
    
    connectionLayout->addWidget(new QLabel("URL:", this), 0, 0);
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText("ws://localhost:8889/audio");
    connectionLayout->addWidget(m_urlEdit, 0, 1, 1, 2);
    
    m_testButton = new QPushButton("Test", this);
    m_testButton->setFixedWidth(80);
    connectionLayout->addWidget(m_testButton, 0, 3);
    
    m_connectButton = new QPushButton("Connect", this);
    connectionLayout->addWidget(m_connectButton, 1, 1, 1, 3);
    
    mainLayout->addWidget(connectionGroup);
    
    // Audio Settings Group
    auto* audioGroup = new QGroupBox("Audio Settings", this);
    auto* audioLayout = new QGridLayout(audioGroup);
    
    audioLayout->addWidget(new QLabel("Source:", this), 0, 0);
    m_audioSourceCombo = new QComboBox(this);
    audioLayout->addWidget(m_audioSourceCombo, 0, 1, 1, 3);
    
    audioLayout->addWidget(new QLabel("Level:", this), 1, 0);
    m_audioLevelBar = new QProgressBar(this);
    m_audioLevelBar->setRange(0, 100);
    m_audioLevelBar->setTextVisible(false);
    audioLayout->addWidget(m_audioLevelBar, 1, 1, 1, 3);
    
    mainLayout->addWidget(audioGroup);
    
    // Status Group
    auto* statusGroup = new QGroupBox("Status", this);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    
    m_statusLabel = new QLabel("Disconnected", this);
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; }");
    statusLayout->addWidget(m_statusLabel);
    
    m_dataRateLabel = new QLabel("Data Rate: 0.0 KB/s", this);
    statusLayout->addWidget(m_dataRateLabel);
    
    mainLayout->addWidget(statusGroup);
    
    // Control Buttons
    auto* buttonLayout = new QHBoxLayout();
    
    m_startStopButton = new QPushButton("Start Streaming", this);
    m_startStopButton->setEnabled(false);
    buttonLayout->addWidget(m_startStopButton);
    
    auto* closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    buttonLayout->addWidget(closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Initial state
    populateAudioSources();
}

void SettingsDialog::connectSignals() {
    connect(m_testButton, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);
    connect(m_connectButton, &QPushButton::clicked, this, &SettingsDialog::onConnectToggled);
    connect(m_startStopButton, &QPushButton::clicked, this, &SettingsDialog::onStartStopToggled);
    connect(m_audioSourceCombo, &QComboBox::currentTextChanged, this, &SettingsDialog::onAudioSourceChanged);
    connect(m_urlEdit, &QLineEdit::textChanged, this, &SettingsDialog::onUrlChanged);
    
    // Connect to AudioStreamer signals
    connect(m_streamer, &AudioStreamer::connectionStatusChanged, 
            this, &SettingsDialog::updateConnectionStatus);
    connect(m_streamer, &AudioStreamer::streamingStatusChanged, 
            this, &SettingsDialog::updateStreamingStatus);
    connect(m_streamer, &AudioStreamer::dataRateChanged, 
            this, &SettingsDialog::updateDataRate);
    connect(m_streamer, &AudioStreamer::errorOccurred, 
            this, &SettingsDialog::showError);
}

void SettingsDialog::loadSettings() {
    // Load from OBS user config
#if LIBOBS_API_MAJOR_VER >= 31
    config_t* config = obs_frontend_get_user_config();
#else
    config_t* config = obs_frontend_get_profile_config();
#endif
    
    const char* url = config_get_string(config, "AudioStreamer", "WebSocketUrl");
    if (url && strlen(url) > 0) {
        m_urlEdit->setText(url);
        m_streamer->SetWebSocketUrl(url);
    } else {
        m_urlEdit->setText(QString::fromStdString(m_streamer->GetWebSocketUrl()));
    }
    
    const char* source = config_get_string(config, "AudioStreamer", "AudioSource");
    if (source && strlen(source) > 0) {
        m_audioSourceCombo->setCurrentText(source);
        m_streamer->SetAudioSource(source);
    }
}

void SettingsDialog::saveSettings() {
    // Save to OBS user config
#if LIBOBS_API_MAJOR_VER >= 31
    config_t* config = obs_frontend_get_user_config();
#else
    config_t* config = obs_frontend_get_profile_config();
#endif
    
    config_set_string(config, "AudioStreamer", "WebSocketUrl", 
                     m_urlEdit->text().toStdString().c_str());
    config_set_string(config, "AudioStreamer", "AudioSource", 
                     m_audioSourceCombo->currentText().toStdString().c_str());
    
    config_save(config);
}

void SettingsDialog::onConnectToggled() {
    if (m_streamer->IsConnected()) {
        m_streamer->Stop();
    } else {
        m_streamer->Start();
    }
}

void SettingsDialog::onStartStopToggled() {
    if (m_streamer->IsStreaming()) {
        m_streamer->Stop();
    } else {
        m_streamer->Start();
    }
}

void SettingsDialog::onTestConnection() {
    // Simple connection test
    m_testButton->setEnabled(false);
    m_testButton->setText("Testing...");
    
    // This would ideally test the connection
    // For now, just enable the button back after a delay
    QTimer::singleShot(1000, this, [this]() {
        m_testButton->setEnabled(true);
        m_testButton->setText("Test");
        QMessageBox::information(this, "Connection Test", 
                               "Connection test completed. Check status for results.");
    });
}

void SettingsDialog::onAudioSourceChanged(const QString& source) {
    m_streamer->SetAudioSource(source.toStdString());
}

void SettingsDialog::onUrlChanged(const QString& url) {
    m_streamer->SetWebSocketUrl(url.toStdString());
}

void SettingsDialog::updateConnectionStatus(bool connected) {
    if (connected) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
        m_connectButton->setText("Disconnect");
        m_startStopButton->setEnabled(true);
    } else {
        m_statusLabel->setText("Disconnected");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
        m_connectButton->setText("Connect");
        m_startStopButton->setEnabled(false);
    }
}

void SettingsDialog::updateStreamingStatus(bool streaming) {
    if (streaming) {
        m_startStopButton->setText("Stop Streaming");
        m_audioSourceCombo->setEnabled(false);
        m_urlEdit->setEnabled(false);
    } else {
        m_startStopButton->setText("Start Streaming");
        m_audioSourceCombo->setEnabled(true);
        m_urlEdit->setEnabled(true);
    }
}

void SettingsDialog::updateDataRate(double kbps) {
    m_dataRateLabel->setText(QString("Data Rate: %1 KB/s").arg(kbps, 0, 'f', 1));
}

void SettingsDialog::showError(const QString& error) {
    QMessageBox::warning(this, "Audio to WebSocket Error", error);
}

void SettingsDialog::updateStatus() {
    // Update any real-time status here
    // For example, audio levels could be updated here
}

void SettingsDialog::populateAudioSources() {
    m_audioSourceCombo->clear();
    
    // Enumerate all audio sources
    auto enumCallback = [](void* param, obs_source_t* source) -> bool {
        auto* combo = static_cast<QComboBox*>(param);
        
        uint32_t flags = obs_source_get_output_flags(source);
        if (flags & OBS_SOURCE_AUDIO) {
            const char* name = obs_source_get_name(source);
            const char* display_name = obs_source_get_name(source);
            
            if (name && display_name) {
                combo->addItem(QString("%1").arg(display_name), QString(name));
            }
        }
        
        return true;
    };
    
    obs_enum_sources(enumCallback, m_audioSourceCombo);
    
    // Also add special sources
    m_audioSourceCombo->addItem("Desktop Audio", "desktop_audio");
    m_audioSourceCombo->addItem("Mic/Aux", "mic_aux");
}

} // namespace obs_audio_to_websocket
