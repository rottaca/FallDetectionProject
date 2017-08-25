#ifndef SIMPLETIMEPLOT_H
#define SIMPLETIMEPLOT_H

#include <QWidget>
#include <QPaintEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QPen>

#include <assert.h>
#include <map>


class SimpleTimePlot : public QWidget
{
    Q_OBJECT
public:
    explicit SimpleTimePlot(QWidget *parent = 0);

    void paintEvent(QPaintEvent* event);

    void addPoint(double x, double y)
    {
        if(!ValidDouble(x))
            return;
        QMutexLocker locker(&m_dataMutex);
        m_data.insert(std::make_pair(x,y));
        m_xMin = x - m_xRange;
        m_xMax = x;
    }

    void addLine(int groupIdx, double x, QPen p)
    {
        if(!ValidDouble(x))
            return;
        QMutexLocker locker(&m_lineMutex);

        if(m_lines.find(groupIdx) == m_lines.end()) {
            m_lines[groupIdx] = LineGroup();
            m_lines[groupIdx].pen = p;
            m_lines[groupIdx].active = true;
        }
        LineGroup &lg = m_lines[groupIdx];
        if(std::find(lg.positions.begin(),lg.positions.end(),x) == lg.positions.end())
            lg.positions.push_back(x);
    }

    void setLineGroupActive(int groupIdx,bool active)
    {
        if(m_lines.find(groupIdx) != m_lines.end()) {
            m_lines[groupIdx].active = active;
        }
    }

    void setYRange(double yMin, double yMax)
    {
        if(!ValidDouble(yMin) || !ValidDouble(yMin))
            return;
        QMutexLocker locker(&m_dataMutex);
        assert(yMin < yMax);
        m_yMin = yMin;
        m_yMax = yMax;
    }

    void setXRange(double xRange)
    {
        if(!ValidDouble(xRange))
            return;
        QMutexLocker locker(&m_dataMutex);
        m_xRange = xRange;
    }

    void setTitle(QString title)
    {
        QMutexLocker locker(&m_dataMutex);
        m_tite = title;
    }

    void cleanupMap();

    void clear()
    {
        {
            QMutexLocker locker(&m_dataMutex);
            m_data.clear();
        }
        {
            QMutexLocker locker(&m_lineMutex);
            m_lines.clear();
        }
    }

private:
    QRect drawFrame(QPainter *painter);

    bool ValidDouble(double value)
    {
        if (value != value) {
            return false;
        } else if (value > std::numeric_limits<qreal>::max()) {
            return false;
        } else if (value < -std::numeric_limits<qreal>::max()) {
            return false;
        } else
            return true;
    }

private:
    QMutex m_dataMutex;
    double m_xRange;
    double m_xMin, m_xMax, m_yMin, m_yMax;
    std::map<double, double> m_data;

    QMutex m_lineMutex;
    typedef struct LineGroup {
        std::vector<double> positions;
        QPen pen;
        bool active;
    } LineGroup;
    std::map<int,LineGroup> m_lines;
    QString m_tite;
};
#endif // SIMPLETIMEPLOT_H
