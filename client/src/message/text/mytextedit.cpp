#include "mytextedit.h"
#include <QVBoxLayout>
#include <QStringListModel>
#include <QDebug>
#include <QAbstractItemView>
#include <QScrollBar>

Completer::Completer(QWidget *parent)
: QCompleter(parent) { }

MyTextEdit::MyTextEdit(QWidget *parent)
: QWidget(parent) {
    initUI();
    initConnect();
    completer = nullptr;
}

void MyTextEdit::initConnect() const {
    /*当输入文本内容发生改变的时候*/
    connect(edit, &QPlainTextEdit::textChanged, this, &MyTextEdit::complete);
}

void MyTextEdit::initUI() {
    QVBoxLayout *layout = new QVBoxLayout(this); //创建垂直布局
    layout->setContentsMargins(0, 0, 0, 0); //设置布局中上下左右边界
    edit = new QPlainTextEdit(); //创建输入框控件
    /*设置输入框控件中提示字段*/
    edit->setPlaceholderText(QString::fromUtf8("点击发送信息..."));
    /*将控件加入布局中*/
    layout->addWidget(edit);
    /*
    在 edit 控件自己处理任何事件（如鼠标点击、键盘按键）之前，
    先让 this 拦截并检查一遍。 
    这允许你修改、拦截甚至完全丢弃发往 edit 的事件
    */
    edit->installEventFilter(this);
}

QString MyTextEdit::textUnderCursor() {
    QTextCursor tc = edit->textCursor();
    /*选择光标下的整个单词*/
    tc.select(QTextCursor::WordUnderCursor);
    /*返回选中的文本*/
    return tc.selectedText();
}

void MyTextEdit::complete(){
    /*如果文本为空或completer为空，则返回*/
    if(edit->toPlainText().size() == 0 || completer == nullptr) return;
    /*获取文本最后一个字符*/
    QChar tail =  edit->toPlainText().at(edit->toPlainText().size() - 1);
    /*如果最后一个字符为@，则显示补全*/
    if(tail == '@') {
        /*设置补全前缀*/
        completer->setCompletionPrefix(tail);
        /*获取补全弹窗视图对象*/
        QAbstractItemView *view = completer->popup();
        /*设置当前索引*/
        view->setCurrentIndex(completer->completionModel()->index(0, 0));
        /*获取光标矩形*/
        QRect cr = edit->cursorRect();
        /*获取垂直滚动条*/
        QScrollBar *bar = completer->popup()->verticalScrollBar();
        /*设置矩形宽度,列表的宽度加上滚动条的宽度*/
        cr.setWidth(completer->popup()->sizeHintForColumn(0) + bar->sizeHint().width());
        /*显示补全*/
        completer->complete(cr);
    }
}

void MyTextEdit::changeCompletion(const QString &text) {
    /*获取光标*/
    QTextCursor tc = edit->textCursor();
    /*获取文本长度*/
    int len = text.size() - completer->completionPrefix().size();
    /*将光标移动到单词末尾*/
    tc.movePosition(QTextCursor::EndOfWord);
    /*插入文本,从右往左截取len个字符*/
    tc.insertText(text.right(len));
    /*设置光标*/
    edit->setTextCursor(tc);
    /*隐藏补全弹窗*/
    completer->popup()->hide();

    QString str = edit->toPlainText();
    /*获取文本最后一个字符的位置*/
    int pos = str.size() - 1;
    /*如果最后一个字符不为@，则继续往前找*/
    while(str.at(pos) != '@') pos--;

    /*清除当前的文本选中状态（取消高亮）但不会删除被选中的文本内容*/
    tc.clearSelection();
    /*设置光标位置*/
    tc.setPosition(pos, QTextCursor::MoveAnchor);
    /*设置光标选中范围*/
    tc.setPosition(str.size(), QTextCursor::KeepAnchor);
    /*设置文本格式*/
    QTextCharFormat fmt = tc.charFormat();
    QTextCharFormat fmt_back = fmt;
    fmt.setForeground(QBrush(Qt::white));
    fmt.setBackground(QBrush(QColor(0, 160, 233)));
    tc.setCharFormat(fmt);
    tc.clearSelection();
    tc.setCharFormat(fmt_back);

    tc.insertText(" ");
    edit->setTextCursor(tc);

    /*将[@... ]的文本范围添加到ipspan中,方便之后删除为整块删除*/
    ipspan.push_back(std::make_pair(pos, str.size() + 1));

}

QString MyTextEdit::toPlainText() {
    return edit->toPlainText();
}

void MyTextEdit::setPlainText(QString str) {
    edit->setPlainText(str);
}

void MyTextEdit::setPlaceholderText(QString str) {
    edit->setPlaceholderText(str);
}

void MyTextEdit::setCompleter(const std::vector<QString> &items) {
    //如果completer为空,则创建一个completer
    if(completer == nullptr) {
        completer = new Completer(this);
        completer->setWidget(this); //指定补全弹窗在哪个窗口显示
        completer->setCompletionMode(QCompleter::PopupCompletion);//使用下拉列表弹出选项
        completer->setCaseSensitivity(Qt::CaseInsensitive);//不区分大小写
        //当用户选择一个选项时,调用changeCompletion槽函数
        connect(completer, qOverload<const QString &>(&QCompleter::activated), 
                this, &MyTextEdit::changeCompletion);
    } else {
        delete completer->model(); //删除原来的模型
        //就是上次传入的stringlist
    }
    /*创建一个QStringList*/
    QStringList stringlist;
    /*预分配空间*/
    stringlist.reserve(static_cast<int>(items.size()));
    /*将items中的每个字符串添加到stringlist中*/
    for (const QString &item : items)
        stringlist.append(item);
    /*使用这次传入的stringlist创建一个新模型*/
    QStringListModel * model = new QStringListModel(stringlist, this);
    /*设置模型*/
    completer->setModel(model);
}

bool MyTextEdit::eventFilter(QObject *obj, QEvent *event) {
    /*如果对象不是edit，则不进行拦截*/
    if(obj != edit) 
        return QWidget::eventFilter(obj, event);
    /*如果事件不是键盘事件，则不进行拦截*/
    if(event->type() != QEvent::KeyPress) 
        return QWidget::eventFilter(obj, event);
    /*获得键盘事件*/ 
    QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
    /*获取光标*/
    QTextCursor tc = edit->textCursor();
    /*获取光标原始位置*/
    int p = tc.position();
    /*遍历ipspan*/
    for(int i = 0; i < ipspan.size(); i++) {
        /*如果键盘事件是退格键且光标位置在ipspan范围内，则删除整块文本*/
        if( (keyevent->key() == Qt::Key_Backspace 
            && p > ipspan[i].first 
            && p <= ipspan[i].second ) || 
            /*如果键盘事件是删除键且光标位置在ipspan范围内，则删除整块文本*/
            (keyevent->key() == Qt::Key_Delete 
            && p >= ipspan[i].first 
            && p < ipspan[i].second) ) 
        {
            /*设置光标位置为ipspan[i]起始位置,不选中任何文本*/
            tc.setPosition(ipspan[i].first, QTextCursor::MoveAnchor);
            /*如果光标位置在ipspan[i]末尾，则设置光标选中范围*/
            if(p == ipspan[i].second) {
                /*设置光标选中范围为ipspan[i]末尾*/
                tc.setPosition(ipspan[i].second, QTextCursor::KeepAnchor);
            } else {
                /*光标在ipspan[i]中间*/
                /*设置光标选中范围为ipspan[i]末尾+1*/
                tc.setPosition(ipspan[i].second + 1, QTextCursor::KeepAnchor);
            }
            /*删除选中文本*/
            tc.removeSelectedText();
            /*删除ipspan[i]*/
            ipspan.erase(ipspan.begin() + i);
            /*返回true,表示事件已被处理*/
            return true;
        }
        else if(p >= ipspan[i].first && p <= ipspan[i].second) {
            QTextCursor tc = edit->textCursor();
            tc.setPosition(ipspan[i].second);
            edit->setTextCursor(tc);
        }
    }
    return QWidget::eventFilter(obj, event);
}
