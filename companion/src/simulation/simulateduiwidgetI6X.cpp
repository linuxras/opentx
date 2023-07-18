#include "simulateduiwidget.h"
#include "ui_simulateduiwidgetI6X.h"

// NOTE: RadioUiAction(NUMBER,...): NUMBER relates to enum EnumKeys in the specific board.h

SimulatedUIWidgetI6X::SimulatedUIWidgetI6X(SimulatorInterface *simulator, QWidget * parent):
  SimulatedUIWidget(simulator, parent),
  ui(new Ui::SimulatedUIWidgetI6X)
{
  RadioUiAction * act;

  ui->setupUi(this);

  const int bh = 18, bw = 41;
  // add actions in order of appearance on the help menu
  // KEY_MENU 0
  // KEY_ENTER 0
  // KEY_EXIT 1
  // KEY_DOWN 2
  // KEY_MINUS 2
  // KEY_UP 3
  // KEY_PLUS 3
  // KEY_RIGHT 4
  // KEY_LEFT 5
  // KEY_BIND 6


  act = new RadioUiAction(3, QList<int>() << Qt::Key_Plus  << Qt::Key_Left, SIMU_STR_HLP_KEYS_GO_UP, SIMU_STR_HLP_ACT_UP);
  addRadioWidget(ui->leftbuttons->addArea(QRect(108, 46, bw, bh), "I6X/left_up.png", act));

  act = new RadioUiAction(2, QList<int>() << Qt::Key_Minus << Qt::Key_Right, SIMU_STR_HLP_KEYS_GO_DN, SIMU_STR_HLP_ACT_DN);
  addRadioWidget(ui->leftbuttons->addArea(QRect(108, 87, bw, bh), "I6X/left_down.png", act));

  act = new RadioUiAction(4, QList<int>() << Qt::Key_PageDown, SIMU_STR_HLP_KEY_PGDN, SIMU_STR_HLP_ACT_PAGE);
  addRadioWidget(ui->leftbuttons->addArea(QRect(134, 163, 25, 25), "I6X/left_bind.png", act));


  act = new RadioUiAction(0, QList<int>() << Qt::Key_Enter << Qt::Key_Return, SIMU_STR_HLP_KEYS_ACTIVATE, SIMU_STR_HLP_ACT_ENT);
  addRadioWidget(ui->rightbuttons->addArea(QRect(47, 46, bw, bh), "I6X/right_ok.png", act));

  act = new RadioUiAction(1, QList<int>() << Qt::Key_Delete << Qt::Key_Escape << Qt::Key_Backspace, SIMU_STR_HLP_KEYS_EXIT, SIMU_STR_HLP_ACT_EXIT);
  addRadioWidget(ui->rightbuttons->addArea(QRect(47, 87, bw, bh), "I6X/right_cancel.png", act));


  m_backlightColors << QColor(215, 243, 255);  // X7 Blue
  m_backlightColors << QColor(166,247,159);
  m_backlightColors << QColor(247,159,166);
  m_backlightColors << QColor(255,195,151);
  m_backlightColors << QColor(247,242,159);

  setLcd(ui->lcd);

/*  QString css = "#radioUiWidget {"
                "background-color: qlineargradient(spread:reflect, x1:0, y1:0, x2:0, y2:1,"
                "stop:0 rgba(255, 255, 255, 255),"
                "stop:0.757062 rgba(241, 238, 238, 255),"
                "stop:1 rgba(247, 245, 245, 255));"
                "}";

  QTimer * tim = new QTimer(this);
  tim->setSingleShot(true);
  connect(tim, &QTimer::timeout, [this, css]() {
    emit customStyleRequest(css);
  });
  tim->start(100);*/
}

SimulatedUIWidgetI6X::~SimulatedUIWidgetI6X()
{
  delete ui;
}
