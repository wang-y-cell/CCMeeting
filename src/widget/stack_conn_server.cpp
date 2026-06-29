#include "stack_conn_server.h"
#include "ui_stack_conn_server.h"
#include <QPushButton>

stack_conn_server::stack_conn_server(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::stack_conn_server)
{
    ui->setupUi(this);
    ui->ip_line_edit->setText("127.0.0.1");
    ui->port_line_edit->setText("8888");

    connect(ui->connServer_btn, &QPushButton::clicked, this, [this]() {
        emit ConnServerClicked(ui->ip_line_edit->text().trimmed(),
                               ui->port_line_edit->text().trimmed());
    });
}

stack_conn_server::~stack_conn_server()
{
    delete ui;
}
