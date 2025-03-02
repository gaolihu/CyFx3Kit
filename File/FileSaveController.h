#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include "FileSaveManager.h"
#include "FileSavePanel.h"
#include "SaveFileBox.h"

/**
 * @brief Controller class for file saving functionality
 *
 * This class handles all business logic related to file saving operations,
 * managing the interaction between UI components and the FileSaveManager.
 * Follows the MVC pattern to separate concerns.
 */
class FileSaveController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent QObject
     */
    explicit FileSaveController(QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~FileSaveController();

    /**
     * @brief Initialize file saving components
     * @return True if initialization successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Create a FileSavePanel widget
     * @param parent Parent widget
     * @return Pointer to created FileSavePanel
     */
    FileSavePanel* createFileSavePanel(QWidget* parent);

    /**
     * @brief Create a SaveFileBox dialog
     * @param parent Parent widget
     * @return Pointer to created SaveFileBox
     */
    SaveFileBox* createSaveFileBox(QWidget* parent);

    /**
     * @brief Set image parameters for saving
     * @param width Image width
     * @param height Image height
     * @param format Image format code
     */
    void setImageParameters(uint16_t width, uint16_t height, uint8_t format);

    /**
     * @brief Check if file saving is in progress
     * @return True if saving, false otherwise
     */
    bool isSaving() const;

public slots:
    /**
     * @brief Start the file saving process
     */
    void startSaving();

    /**
     * @brief Stop the file saving process
     */
    void stopSaving();

    /**
     * @brief Handle data packet for saving
     * @param packet Data packet to save
     */
    void processDataPacket(const DataPacket& packet);

signals:
    /**
     * @brief Signal emitted when saving completes
     * @param path Path where file was saved
     * @param totalBytes Total bytes saved
     */
    void saveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief Signal emitted when a saving error occurs
     * @param error Error message
     */
    void saveError(const QString& error);

    /**
     * @brief Signal emitted when saving status changes
     * @param status New saving status
     */
    void saveStatusChanged(SaveStatus status);

private slots:
    /**
     * @brief Handle save manager completion event
     * @param path Path where file was saved
     * @param totalBytes Total bytes saved
     */
    void onSaveManagerCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief Handle save manager error event
     * @param error Error message
     */
    void onSaveManagerError(const QString& error);

private:
    /**
     * @brief Connect signals and slots
     */
    void connectSignals();

    /**
     * @brief Validate image parameters
     * @param width Image width
     * @param height Image height
     * @param capType Image format code
     * @return True if parameters are valid, false otherwise
     */
    bool validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& capType);

    // Image parameters
    uint16_t m_width;
    uint16_t m_height;
    uint8_t m_format;

    // UI components (weak references, not owned)
    FileSavePanel* m_fileSavePanel;
    SaveFileBox* m_saveFileBox;

    // Flag indicating if initialized
    bool m_initialized;
};