#ifndef FORGOTPAGE_H
#define FORGOTPAGE_H

#include <QWidget>

namespace Ui {
class ForgotPage;
}

class ForgotPage : public QWidget
{
    Q_OBJECT

public:
    explicit ForgotPage(QWidget *parent = nullptr);
    ~ForgotPage();

private:
    Ui::ForgotPage *ui;
};

#endif // FORGOTPAGE_H
