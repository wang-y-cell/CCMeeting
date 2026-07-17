#ifndef MYTEXTEDIT_H
#define MYTEXTEDIT_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QCompleter>
#include <QPair>
#include <utility>
#include <vector>

/**
 * @brief 用于 @ 成员补全的 Completer
 */
class Completer
: public QCompleter {
    Q_OBJECT
public:
    /**
     * @brief 构造补全器
     * @param parent 父控件
     */
    explicit Completer(QWidget *parent = nullptr);
};

/**
 * @brief 会议聊天输入框（由 QWidget 提升）
 *
 * 内部组合 QPlainTextEdit，支持：
 * - 输入 @ 弹出成员补全
 * - @IP 段高亮
 * - Backspace/Delete 整块删除 @ 段
 */
class MyTextEdit
: public QWidget {
    Q_OBJECT
private:
    QPlainTextEdit *edit; ///< 实际文本输入控件
    Completer *completer; ///< @ 补全器，懒创建
    std::vector<std::pair<int, int>> ipspan; ///< 各 @IP 段在文本中的 [first, second) 范围

public:
    /**
     * @brief 构造输入框
     * @param parent 父控件
     */
    explicit MyTextEdit(QWidget *parent = nullptr);

    /** @brief 返回输入框纯文本 */
    QString toPlainText();
    /**
     * @brief 设置输入框文本
     * @param str 文本内容
     */
    void setPlainText(QString str);
    /**
     * @brief 设置占位提示
     * @param str 提示文案
     */
    void setPlaceholderText(QString str);
    /**
     * @brief 是否滚动时将光标行居中
     * @param on true 开启
     */
    void setCenterOnScroll(bool on) { edit->setCenterOnScroll(on); }
    /**
     * @brief 设置/更新 @ 补全列表
     * @param items 补全项（如 "@192.168.1.1"）
     */
    void setCompleter(const std::vector<QString> &items);

private:
    /** @brief 连接 textChanged 等信号（completer 需在创建后再连 activated） */
    void initConnect() const;
    /** @brief 创建内部 QPlainTextEdit 与布局 */
    void initUI();
    /** @brief 返回光标下当前单词 */
    QString textUnderCursor();
    /**
     * @brief 拦截编辑器按键，实现 @ 段原子删除
     * @param obj 事件对象
     * @param event 事件
     * @return true 表示已处理
     */
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    /**
     * @brief 用户选中补全项后插入并高亮
     * @param text 选中的完整补全字符串
     */
    void changeCompletion(const QString &text);

public slots:
    /** @brief 文本变化时，若末尾为 @ 则弹出补全 */
    void complete();

signals:
};

#endif // MYTEXTEDIT_H
