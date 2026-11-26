#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QtTest>

#include "ui/shell/main_window_ui.h"

/**
 * @brief 主窗口骨架 UI 的结构回归
 *
 * 需求：左上角用户卡改成横排（左头像，右「昵称 + 简介」）；
 * 账号操作（退出登录 / 注销账号）放在个人资料页，不占侧栏卡片。
 */
class TestMainWindowUi : public QObject {
    Q_OBJECT
private slots:
    void user_card_is_horizontal_with_info() {
        QWidget parent;
        MainWindowUi ui;
        ui.setupUi(&parent);

        QVERIFY(ui.userCard != nullptr);
        QVERIFY(ui.avatarLabel != nullptr);
        QVERIFY(ui.nameLabel != nullptr);
        QVERIFY(ui.infoLabel != nullptr);

        // 横排布局：头像一个 44×44 的方块，昵称/简介在右侧文字列
        auto *layout = qobject_cast<QHBoxLayout *>(ui.userCard->layout());
        QVERIFY2(layout != nullptr, "用户卡应为横排（头像 + 昵称/简介）");
        QCOMPARE(ui.avatarLabel->width(), 44);
        QCOMPARE(ui.avatarLabel->height(), 44);
        QCOMPARE(ui.nameLabel->parentWidget(), ui.userCard);
        QCOMPARE(ui.infoLabel->parentWidget(), ui.userCard);
    }

    void user_card_still_clickable() {
        QWidget parent;
        MainWindowUi ui;
        ui.setupUi(&parent);
        // 点卡片空白处仍进资料页：光标保持手型
        QCOMPARE(ui.userCard->cursor().shape(), Qt::PointingHandCursor);
    }
};

QTEST_MAIN(TestMainWindowUi)
#include "test_main_window_ui.moc"
