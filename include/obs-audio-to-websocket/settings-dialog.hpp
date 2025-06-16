#pragma once

#include <QDialog>
#include <QTimer>
#include <memory>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
class QComboBox;
class QLabel;
class QProgressBar;
QT_END_NAMESPACE

namespace obs_audio_to_websocket {

class AudioStreamer;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

private slots:
    void onConnectToggled();
    void onStartStopToggled();
    void onTestConnection();
    void onAudioSourceChanged(const QString& source);
    void onUrlChanged(const QString& url);
    
    void updateConnectionStatus(bool connected);
    void updateStreamingStatus(bool streaming);
    void updateDataRate(double kbps);
    void showError(const QString& error);
    
    void updateStatus();
    void populateAudioSources();

private:
    void setupUi();
    void connectSignals();
    void loadSettings();
    void saveSettings();
    
    // UI Elements
    QLineEdit* m_urlEdit;
    QPushButton* m_testButton;
    QPushButton* m_connectButton;
    QComboBox* m_audioSourceCombo;
    QPushButton* m_startStopButton;
    QLabel* m_statusLabel;
    QLabel* m_dataRateLabel;
    QProgressBar* m_audioLevelBar;
    
    // Update timer
    std::unique_ptr<QTimer> m_updateTimer;
    
    // Reference to audio streamer
    AudioStreamer* m_streamer;
};

} // namespace obs_audio_to_websocket