#ifndef RECORDINGS_PAGE_H
#define RECORDINGS_PAGE_H

#include "ui/pages/page.h"

#include "recording_library.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QScrollArea;

/**
 * @brief 主窗口「录制」页：列出本地录制文件，可回放/删除
 *
 * 每次显示时重新扫描录制目录（RecordingLibrary::scan）。
 */
namespace ui {

class RecordingsPage : public Page {
    Q_OBJECT

public:
    QString page_id() const override { return QStringLiteral("recordings"); }
    QString title() const override { return tr("录制"); }
    QString icon_name() const override { return QStringLiteral("record"); }
    void on_show() override;

    int nav_order() const override { return 30; }
public:
    explicit RecordingsPage(QWidget *parent = nullptr);

    /** @brief 重新扫描并刷新列表 */
    void refresh();

signals:
    /** @brief 请求回放某个录制 */
    void playRequested(const RecordingEntry &entry);

private:
    void build_ui();
    void clear_rows();
    void add_row(const RecordingEntry &entry, int index);
    void open_folder();

    QVBoxLayout *_rowLayout = nullptr;
    QScrollArea *_scroll = nullptr;
    QLabel *_emptyHint = nullptr;
    QLabel *_countLabel = nullptr;
    QPushButton *_refreshBtn = nullptr;
    QPushButton *_folderBtn = nullptr;

    QVector<RecordingEntry> _entries;
};

}  // namespace ui

#endif // RECORDINGS_PAGE_H
