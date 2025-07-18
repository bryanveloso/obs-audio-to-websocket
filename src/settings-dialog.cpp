#include "obs-audio-to-websocket/settings-dialog.hpp"
#include "obs-audio-to-websocket/audio-streamer.hpp"
#include "obs-audio-to-websocket/websocketpp-client.hpp"
#include "obs-audio-to-websocket/obs-source-wrapper.hpp"
#include <chrono>
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
#include <QCloseEvent>
#include <QUrl>
#include <obs.h>
#include <obs-frontend-api.h>

#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(param) (void)param
#endif

namespace obs_audio_to_websocket {

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent), m_streamer(&AudioStreamer::Instance())
{
	setupUi();
	connectSignals();
	loadSettings();

	// Apply default microphone selection if no valid source was loaded
	if (m_audioSourceCombo->currentIndex() < 0 || m_audioSourceCombo->currentText().isEmpty()) {
		selectDefaultMicrophoneSource();
	}

	m_updateTimer = std::make_unique<QTimer>(this);
	connect(m_updateTimer.get(), &QTimer::timeout, this, &SettingsDialog::updateStatus);
	m_updateTimer->start(100); // Update every 100ms
}

SettingsDialog::~SettingsDialog()
{
	if (m_volmeter) {
		obs_volmeter_remove_callback(m_volmeter, volumeCallback, this);
		obs_volmeter_destroy(m_volmeter);
		m_volmeter = nullptr;
	}
}

void SettingsDialog::setupUi()
{
	setWindowTitle("Audio to WebSocket Settings");
	setFixedSize(450, 400);

	auto *mainLayout = new QVBoxLayout(this);

	// Connection Settings Group
	auto *connectionGroup = new QGroupBox("WebSocket Connection", this);
	auto *connectionLayout = new QGridLayout(connectionGroup);

	connectionLayout->addWidget(new QLabel("URL:", this), 0, 0);
	m_urlEdit = new QLineEdit(this);
	m_urlEdit->setPlaceholderText("ws://localhost:8889/audio");
	connectionLayout->addWidget(m_urlEdit, 0, 1, 1, 2);

	m_testButton = new QPushButton("Test Connection", this);
	connectionLayout->addWidget(m_testButton, 0, 3);

	mainLayout->addWidget(connectionGroup);

	// Audio Settings Group
	auto *audioGroup = new QGroupBox("Audio Settings", this);
	auto *audioLayout = new QGridLayout(audioGroup);

	audioLayout->addWidget(new QLabel("Source:", this), 0, 0);
	m_audioSourceCombo = new QComboBox(this);
	audioLayout->addWidget(m_audioSourceCombo, 0, 1, 1, 2);

	m_refreshButton = new QPushButton("Refresh", this);
	m_refreshButton->setMaximumWidth(80);
	connect(m_refreshButton, &QPushButton::clicked, this, &SettingsDialog::populateAudioSources);
	audioLayout->addWidget(m_refreshButton, 0, 3);

	// Audio level indicator
	audioLayout->addWidget(new QLabel("Level:", this), 1, 0);
	m_audioLevelBar = new QProgressBar(this);
	m_audioLevelBar->setRange(0, 100);
	m_audioLevelBar->setValue(0);
	m_audioLevelBar->setTextVisible(false);
	m_audioLevelBar->setStyleSheet("QProgressBar {"
				       "  border: 1px solid #999;"
				       "  border-radius: 3px;"
				       "  background-color: #333;"
				       "}"
				       "QProgressBar::chunk {"
				       "  background-color: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,"
				       "    stop: 0 #00ff00, stop: 0.8 #ffff00, stop: 1 #ff0000);"
				       "  border-radius: 2px;"
				       "}");
	audioLayout->addWidget(m_audioLevelBar, 1, 1, 1, 3);

	mainLayout->addWidget(audioGroup);

	// Status Group
	auto *statusGroup = new QGroupBox("Status", this);
	auto *statusLayout = new QVBoxLayout(statusGroup);

	m_statusLabel = new QLabel("Not Streaming", this);
	m_statusLabel->setStyleSheet("QLabel { font-weight: bold; }");
	statusLayout->addWidget(m_statusLabel);

	m_dataRateLabel = new QLabel("Data Rate: 0.0 KB/s", this);
	statusLayout->addWidget(m_dataRateLabel);

	m_muteStatusLabel = new QLabel("", this);
	m_muteStatusLabel->setStyleSheet("QLabel { color: orange; font-weight: bold; }");
	statusLayout->addWidget(m_muteStatusLabel);

	mainLayout->addWidget(statusGroup);

	// Control Buttons
	auto *buttonLayout = new QHBoxLayout();

	m_startStopButton = new QPushButton("Start Streaming", this);
	// Enable if audio source is selected
	m_startStopButton->setEnabled(false);
	buttonLayout->addWidget(m_startStopButton);

	auto *closeButton = new QPushButton("Close", this);
	connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
	buttonLayout->addWidget(closeButton);

	mainLayout->addLayout(buttonLayout);

	// Initial state
	populateAudioSources();
}

void SettingsDialog::connectSignals()
{
	connect(m_testButton, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);
	connect(m_startStopButton, &QPushButton::clicked, this, &SettingsDialog::onStartStopToggled);
	connect(m_audioSourceCombo, &QComboBox::currentTextChanged, this, &SettingsDialog::onAudioSourceChanged);
	connect(m_urlEdit, &QLineEdit::textChanged, this, &SettingsDialog::onUrlChanged);

	// Connect to AudioStreamer signals
	connect(m_streamer, &AudioStreamer::connectionStatusChanged, this, &SettingsDialog::updateConnectionStatus);
	connect(m_streamer, &AudioStreamer::streamingStatusChanged, this, &SettingsDialog::updateStreamingStatus);
	connect(m_streamer, &AudioStreamer::dataRateChanged, this, &SettingsDialog::updateDataRate);
	connect(m_streamer, &AudioStreamer::errorOccurred, this, &SettingsDialog::showError);
}

void SettingsDialog::loadSettings()
{
	// Load from OBS user config
#if LIBOBS_API_MAJOR_VER >= 31
	config_t *config = obs_frontend_get_user_config();
#else
	config_t *config = obs_frontend_get_profile_config();
#endif

	const char *url = config_get_string(config, "AudioStreamer", "WebSocketUrl");
	if (url && strlen(url) > 0) {
		m_urlEdit->setText(url);
		m_streamer->SetWebSocketUrl(url);
	} else {
		m_urlEdit->setText(QString::fromStdString(m_streamer->GetWebSocketUrl()));
	}

	const char *source = config_get_string(config, "AudioStreamer", "AudioSource");
	if (source && strlen(source) > 0) {
		int index = m_audioSourceCombo->findText(source);
		if (index >= 0) {
			m_audioSourceCombo->setCurrentIndex(index);
			m_streamer->SetAudioSource(source);
			// Enable start button if source is valid
			m_startStopButton->setEnabled(true);
		}
	}
}

void SettingsDialog::saveSettings()
{
	// Validate WebSocket URL
	QString url = m_urlEdit->text().trimmed();
	if (!url.isEmpty() && !url.startsWith("ws://") && !url.startsWith("wss://")) {
		QMessageBox::warning(this, "Invalid URL", "WebSocket URL must start with ws:// or wss://");
		return;
	}

	// Save to OBS user config
#if LIBOBS_API_MAJOR_VER >= 31
	config_t *config = obs_frontend_get_user_config();
#else
	config_t *config = obs_frontend_get_profile_config();
#endif

	std::string urlStdString = url.toStdString();
	std::string audioSourceStdString = m_audioSourceCombo->currentText().toStdString();
	config_set_string(config, "AudioStreamer", "WebSocketUrl", urlStdString.c_str());
	config_set_string(config, "AudioStreamer", "AudioSource", audioSourceStdString.c_str());

	config_save(config);
}

void SettingsDialog::closeEvent(QCloseEvent *event)
{
	saveSettings();
	QDialog::closeEvent(event);
}

void SettingsDialog::onStartStopToggled()
{
	if (m_streamer->IsStreaming()) {
		m_streamer->Stop();
	} else {
		m_streamer->Start();
	}
}

void SettingsDialog::onTestConnection()
{
	QString url = m_urlEdit->text().trimmed();
	if (url.isEmpty()) {
		QMessageBox::warning(this, "No URL", "Please enter a WebSocket URL to test.");
		return;
	}

	// Validate URL format
	if (!url.startsWith("ws://") && !url.startsWith("wss://")) {
		QMessageBox::warning(this, "Invalid URL", "WebSocket URL must start with ws:// or wss://");
		return;
	}

	// Basic URL validation - check for host and path
	QUrl qurl(url);
	if (!qurl.isValid() || qurl.host().isEmpty()) {
		QMessageBox::warning(this, "Invalid URL",
				     "Please enter a valid WebSocket URL.\nExample: ws://localhost:8889/audio");
		return;
	}

	// Test WebSocket connection without affecting current state
	m_testButton->setEnabled(false);
	m_testButton->setText("Testing...");

	// Store original status
	QString originalStatus = m_statusLabel->text();
	QString originalStyle = m_statusLabel->styleSheet();

	// Update status to show testing
	m_statusLabel->setText("Testing connection...");
	m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; }");

	// Create a temporary WebSocket client for testing
	auto testClient = std::make_shared<WebSocketPPClient>();

	// Capture error messages
	QString errorMsg;
	testClient->SetOnError([&errorMsg](const std::string &error) {
		errorMsg = QString::fromStdString(error);
		blog(LOG_WARNING, "[Audio to WebSocket] Test connection error: %s", error.c_str());
	});

	testClient->Connect(url.toStdString());

	QTimer::singleShot(2000, this, // 2 second timeout
			   [this, testClient, originalStatus, originalStyle, errorMsg]() {
				   m_testButton->setEnabled(true);
				   m_testButton->setText("Test Connection");

				   if (testClient->IsConnected()) {
					   testClient->Disconnect();
					   m_statusLabel->setText("Test successful!");
					   m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
					   QMessageBox::information(this, "Connection Test",
								    "Connection test successful!");
				   } else {
					   m_statusLabel->setText("Test failed!");
					   m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");

					   QString message = "Connection test failed.";
					   if (!errorMsg.isEmpty()) {
						   message += " " + errorMsg;
					   }
					   QMessageBox::warning(this, "Connection Test", message);
				   }

				   // Restore original status after a delay
				   QTimer::singleShot(2000, this, [this, originalStatus, originalStyle]() {
					   m_statusLabel->setText(originalStatus);
					   m_statusLabel->setStyleSheet(originalStyle);
				   });
			   });
}

void SettingsDialog::onAudioSourceChanged(const QString &source)
{
	m_streamer->SetAudioSource(source.toStdString());
	// Enable/disable start button based on whether source is selected
	if (!m_streamer->IsStreaming()) {
		m_startStopButton->setEnabled(!source.isEmpty());
	}
}

void SettingsDialog::onUrlChanged(const QString &url)
{
	m_streamer->SetWebSocketUrl(url.toStdString());
}

void SettingsDialog::updateConnectionStatus(bool connected)
{
	// Update status based on both connection and streaming state
	if (m_streamer->IsStreaming()) {
		if (connected) {
			m_statusLabel->setText("Streaming (Connected)");
			m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
		} else {
			// Check if we're in reconnection phase
			auto wsClient = m_streamer->GetWebSocketClient();
			if (wsClient && wsClient->IsReconnecting()) {
				int attempts = wsClient->GetReconnectAttempts();
				m_statusLabel->setText(
					QString("Streaming (Reconnecting... attempt %1/10)").arg(attempts));
				m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: orange; }");
			} else {
				m_statusLabel->setText("Streaming (Disconnected)");
				m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
			}
		}
	} else {
		m_statusLabel->setText("Not Streaming");
		m_statusLabel->setStyleSheet("QLabel { font-weight: bold; }");
	}
}

void SettingsDialog::updateStreamingStatus(bool streaming)
{
	if (streaming) {
		m_startStopButton->setText("Stop Streaming");
		// Keep button enabled so user can stop streaming
		m_startStopButton->setEnabled(true);
		// Disable changing settings while streaming
		m_audioSourceCombo->setEnabled(false);
		m_refreshButton->setEnabled(false);
		m_urlEdit->setEnabled(false);
		m_testButton->setEnabled(false);
	} else {
		m_startStopButton->setText("Start Streaming");
		// Re-enable controls when not streaming
		m_audioSourceCombo->setEnabled(true);
		m_refreshButton->setEnabled(true);
		m_urlEdit->setEnabled(true);
		m_testButton->setEnabled(true);
		// Start button enabled when audio source is selected
		m_startStopButton->setEnabled(!m_audioSourceCombo->currentText().isEmpty());
	}

	// Update the status label to reflect streaming state
	updateConnectionStatus(m_streamer->IsConnected());
}

void SettingsDialog::updateDataRate(double kbps)
{
	m_dataRateLabel->setText(QString("Data Rate: %1 kb/s").arg(kbps, 0, 'f', 1));
}

void SettingsDialog::showError(const QString &error)
{
	// Rate limit error dialogs to prevent spam
	auto now = std::chrono::steady_clock::now();
	auto timeSinceLastError = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastErrorTime);

	// Skip if same error within 5 seconds
	if (error == m_lastErrorMessage && timeSinceLastError.count() < 5) {
		return;
	}

	m_lastErrorTime = now;
	m_lastErrorMessage = error;

	// Handle "max reconnection attempts" specially - this should stop streaming
	if (error.contains("Max reconnection attempts exceeded")) {
		m_statusLabel->setText("Not Streaming (Connection Failed)");
		m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
		// Show one final dialog to inform the user
		QMessageBox::warning(this, "Connection Lost", "Connection failed. Streaming stopped.");
		return;
	}

	// Only show dialog boxes for critical errors, not connection failures during streaming
	// Connection status is already shown in the UI status label
	if (!m_streamer->IsStreaming()) {
		// Show dialog only when not actively streaming (e.g., during initial setup)
		QMessageBox::warning(this, "Audio to WebSocket Error", error);
	} else {
		// During streaming, just update the status label instead of showing a dialog
		m_statusLabel->setText("Streaming (Connection Error)");
		m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
		blog(LOG_WARNING, "[Audio to WebSocket] Error during streaming: %s", error.toStdString().c_str());
	}
}

void SettingsDialog::updateStatus()
{
	// Update audio level and mute status
	if (!m_audioSourceCombo->currentText().isEmpty()) {
		obs_source_t *source = obs_get_source_by_name(m_audioSourceCombo->currentText().toStdString().c_str());
		if (source) {
			// Update volume meter attachment
			if (m_volmeter) {
				obs_volmeter_detach_source(m_volmeter);
				obs_volmeter_destroy(m_volmeter);
				m_volmeter = nullptr;
			}

			// Create and attach new volmeter
			m_volmeter = obs_volmeter_create(OBS_FADER_LOG);
			obs_volmeter_add_callback(m_volmeter, volumeCallback, this);
			obs_volmeter_attach_source(m_volmeter, source);

			// Show current peak level
			float db = m_currentPeak;
			if (db < -60.0f)
				db = -60.0f;
			if (db > 0.0f)
				db = 0.0f;

			// Convert to 0-100 range
			int level = static_cast<int>((db + 60.0f) / 60.0f * 100.0f);
			m_audioLevelBar->setValue(level);

			// Check mute status
			if (m_streamer->IsStreaming()) {
				bool muted = obs_source_muted(source);
				if (muted) {
					m_muteStatusLabel->setText("⚠️ Audio source is MUTED");
					m_muteStatusLabel->show();
				} else {
					m_muteStatusLabel->hide();
				}
			} else {
				m_muteStatusLabel->hide();
			}

			obs_source_release(source);
		} else {
			m_audioLevelBar->setValue(0);
			m_muteStatusLabel->hide();

			// Clean up volmeter if source not found
			if (m_volmeter) {
				obs_volmeter_remove_callback(m_volmeter, volumeCallback, this);
				obs_volmeter_destroy(m_volmeter);
				m_volmeter = nullptr;
			}
		}
	} else {
		m_audioLevelBar->setValue(0);
		m_muteStatusLabel->hide();

		// Clean up volmeter if no source selected
		if (m_volmeter) {
			obs_volmeter_remove_callback(m_volmeter, volumeCallback, this);
			obs_volmeter_destroy(m_volmeter);
			m_volmeter = nullptr;
		}
	}
}

void SettingsDialog::populateAudioSources()
{
	// Save current selection
	QString currentSelection = m_audioSourceCombo->currentText();

	m_audioSourceCombo->clear();

	// Collect sources in vectors for sorting
	struct AudioSourceInfo {
		QString name;
		QString id;
		int priority; // Lower number = higher priority
	};
	std::vector<AudioSourceInfo> sources;

	// Enumerate all audio sources
	auto enumCallback = [](void *param, obs_source_t *source) -> bool {
		auto *sources = static_cast<std::vector<AudioSourceInfo> *>(param);

		uint32_t flags = obs_source_get_output_flags(source);
		if (flags & OBS_SOURCE_AUDIO) {
			const char *id = obs_source_get_id(source);
			const char *name = obs_source_get_name(source);

			if (name && id) {
				AudioSourceInfo info;
				info.name = QString(name);
				info.id = QString(id);

				// Prioritize microphones and audio inputs
				if (strstr(id, "input_capture") || strstr(id, "mic")) {
					info.priority = 1;
				} else if (strstr(id, "output_capture")) {
					info.priority = 2;
				} else if (strcmp(id, "browser_source") == 0) {
					info.priority = 4;
				} else {
					info.priority = 3;
				}

				sources->push_back(info);
			}
		}

		return true;
	};

	obs_enum_sources(enumCallback, &sources);

	// Sort by priority, then by name
	std::sort(sources.begin(), sources.end(), [](const AudioSourceInfo &a, const AudioSourceInfo &b) {
		if (a.priority != b.priority)
			return a.priority < b.priority;
		return a.name < b.name;
	});

	// Add sorted sources
	for (const auto &source : sources) {
		m_audioSourceCombo->addItem(source.name, source.name);
	}

	// Restore previous selection if it still exists
	if (!currentSelection.isEmpty()) {
		int index = m_audioSourceCombo->findText(currentSelection);
		if (index >= 0) {
			m_audioSourceCombo->setCurrentIndex(index);
		} else {
			// Source no longer exists, show warning
			m_statusLabel->setText("Previous source not found");
			m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: orange; }");
			QTimer::singleShot(3000, this, [this]() { updateConnectionStatus(m_streamer->IsConnected()); });
		}
	}
}

void SettingsDialog::selectDefaultMicrophoneSource()
{
	// Look for any microphone source
	for (int i = 0; i < m_audioSourceCombo->count(); ++i) {
		QString itemText = m_audioSourceCombo->itemText(i);
		if (itemText.contains("mic", Qt::CaseInsensitive) || itemText.contains("input", Qt::CaseInsensitive)) {
			m_audioSourceCombo->setCurrentIndex(i);
			return;
		}
	}
}

void SettingsDialog::volumeCallback(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				    const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(magnitude);
	UNUSED_PARAMETER(inputPeak);

	auto *dialog = static_cast<SettingsDialog *>(data);

	// Use the highest peak from all channels
	float maxPeak = -60.0f;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (peak[i] > maxPeak) {
			maxPeak = peak[i];
		}
	}

	dialog->m_currentPeak = maxPeak;
}

} // namespace obs_audio_to_websocket
