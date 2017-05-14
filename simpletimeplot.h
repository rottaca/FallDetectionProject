#ifndef SIMPLETIMEPLOT_H
#define SIMPLETIMEPLOT_H

#include <QWidget>
#include <QPaintEvent>

#include <set>

class SimpleTimePlot : public QWidget
{
    Q_OBJECT
public:
    explicit SimpleTimePlot(QWidget *parent = 0);

    void paintEvent(QPaintEvent* event);

    void addPoint(double x, double y)
    {
        m_data.insert(std::make_pair(x,y));
    }

    void setRange(float xMin, double xMax, double yMin, double yMax)
    {
        m_xMin = xMin;
        m_xMax = xMax;
        m_yMin = yMin;
        m_yMax = yMax;
    }

private:
    QRect drawFrame(QPainter *painter);

private:
    double m_xMin, m_xMax, m_yMin, m_yMax;
    std::map<double, double> m_data;
};
#endif // SIMPLETIMEPLOT_H
