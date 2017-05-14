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
        m_xMin = x - m_xRange;
        m_xMax = x;
    }

    void setYRange(double yMin, double yMax)
    {
        m_yMin = yMin;
        m_yMax = yMax;
    }

    void setXRange(double xRange)
    {
        m_xRange = xRange;
    }

    void setTitle(QString title)
    {
        m_tite = title;
    }

private:
    QRect drawFrame(QPainter *painter);

private:
    double m_xRange;
    double m_xMin, m_xMax, m_yMin, m_yMax;
    std::map<double, double> m_data;

    QString m_tite;
};
#endif // SIMPLETIMEPLOT_H
