#ifndef MYTEXTEDIT_H
#define MYTEXTEDIT_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QCompleter>
#include <QPair>
#include <utility>
#include <vector>

class Completer: public QCompleter
{
Q_OBJECT
public:
    explicit Completer(QWidget *parent= nullptr);
};

class MyTextEdit : public QWidget
{
    Q_OBJECT
private:
    QPlainTextEdit *edit;
    Completer *completer;
    std::vector<std::pair<int, int>> ipspan;
public:
    explicit MyTextEdit(QWidget *parent = nullptr);
    QString toPlainText();
    void setPlainText(QString);
    void setPlaceholderText(QString);
    void setCenterOnScroll(bool on){edit->setCenterOnScroll(on);}
    void setCompleter(const std::vector<QString> &items);
private:
    QString textUnderCursor();
    bool eventFilter(QObject *, QEvent *);

private slots:
    void changeCompletion(QString);
public slots:

    void complete();
signals:
};

#endif // MYTEXTEDIT_H
