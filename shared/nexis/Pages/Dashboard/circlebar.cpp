#include "circlebar.h"
#include "ui_circlebar.h"
#include "dpi.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"

CircleBar::~CircleBar()
{
    delete ui;
    // mChart is owned by mChartView (via QChartView constructor) which is a
    // child widget of this CircleBar — Qt's parent-child tree handles cleanup.
}

CircleBar::CircleBar(const QString &title, const QStringList &colors, QWidget *parent,
                     AppManager *appManager, SignalMapper *signalMapper) :
    QWidget(parent),
    ui(new Ui::CircleBar),
    mAppManager(appManager ? appManager : AppManager::ins()),
    mSignalMapper(signalMapper ? signalMapper : SignalMapper::ins()),
    mColors(colors),
    mChart(new QChart),
    mChartView(new QChartView(mChart)),
    mSeries(new QPieSeries(this))
{
    ui->setupUi(this);

    ui->lblCircleChartTitle->setText(title);

    init();
}

void CircleBar::init()
{
    QColor transparent("transparent");

    // series settings
    mSeries->setHoleSize(0.67);
    mSeries->setPieSize(165);
    mSeries->setPieStartAngle(-115);
    mSeries->setPieEndAngle(115);
    mSeries->setLabelsVisible(false);
    mSeries->append("Used", 0);
    mSeries->append("Free", 0);
    mSeries->slices().first()->setBorderColor(transparent);
    mSeries->slices().last()->setBorderColor(transparent);
    QConicalGradient gradient;
    gradient.setAngle(115);
    for (int i = 0; i < mColors.count(); ++i) {
        gradient.setColorAt(i, QColor(mColors.at(i)));
    }
    mSeries->slices().first()->setBrush(gradient);

    // chart settings
    mChart->setBackgroundBrush(QBrush(transparent));
    mChart->setContentsMargins(-Dpi::scale(20), -Dpi::scale(20), -Dpi::scale(20), -Dpi::scale(65));
    mChart->addSeries(mSeries);
    mChart->legend()->hide();

    // chartview settings
    mChartView->setRenderHint(QPainter::Antialiasing);

    ui->layoutCircleBar->insertWidget(1, mChartView);

    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme, [=] {
        QSettings *styleValues = mAppManager->getStyleValues();
        mChartView->setBackgroundBrush(QColor(styleValues->value("@circleChartBackgroundColor").toString()));
        mSeries->slices().last()->setColor(styleValues->value("@pageContent").toString()); // trail color
    });
}

void CircleBar::setValue(const int &value, const QString &valueText)
{
    mSeries->slices().first()->setValue(value);
    mSeries->slices().last()->setValue(100 - value);

    ui->lblCircleChartValue->setText(valueText);
}

