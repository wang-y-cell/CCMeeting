#ifndef MYTEXTEDIT_H
#define MYTEXTEDIT_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QCompleter>
#include <QPair>
#include <utility>
#include <vector>

/**
* @param 用来补全信息的类
*/
class Completer
: public QCompleter {
    Q_OBJECT
public:
    explicit Completer(QWidget *parent= nullptr);
};

/**
由QWidget提升上来的控件
主要用来发送消息,@成员的作用
*/
class MyTextEdit 
: public QWidget {
    Q_OBJECT
private:
    /*发送消息控件*/
    QPlainTextEdit *edit;
    /*信息补全*/
    Completer *completer;
    /*ip范围*/
    std::vector<std::pair<int, int>> ipspan;
public:
    explicit MyTextEdit(QWidget *parent = nullptr);
    /*返回输入框中的文本*/
    QString toPlainText(); 
    /*修改输入框中的文本*/
    void setPlainText(QString str); 
    /*设置文本提示词*/
    void setPlaceholderText(QString str);
    void setCenterOnScroll(bool on){edit->setCenterOnScroll(on);}
    /*设置信息补全列表*/
    void setCompleter(const std::vector<QString> &items);
private:
    /*初始化connect*/
    void initConnect() const;
    /*初始化ui界面*/
    void initUI();
    /*返回光标下的文本*/
    QString textUnderCursor();
    /*拦截事件*/
    bool eventFilter(QObject *, QEvent *) override;

private slots:
    void changeCompletion(const QString &);
public slots:
    void complete();
signals:
};

#endif // MYTEXTEDIT_H
