#include "simpletimeplot.h"

#include <QPainter>

SimpleTimePlot::SimpleTimePlot(QWidget *parent) : QWidget(parent),m_tite("Untitled")
{
    this->setMinimumSize(100,100);
}

void SimpleTimePlot::paintEvent(QPaintEvent* event)
{
    QMutexLocker locker(&m_dataMutex);
    cleanupMap();

    QPainter painter(this);
    QRectF plotFrame = drawFrame(&painter);
    QPointF plotFrameLowRight(plotFrame.x()+plotFrame.width(),
                              plotFrame.y()+plotFrame.height());
    painter.setPen(QPen(Qt::black));

    if(m_data.size() <= 1)
        return;

    QVector<QLineF> lines;
    lines.reserve(m_data.size()-1);
    QPointF lastPoint(0,0);
    double x,y;
    bool isOutside = false;
    bool skippedFirst = false;
    double dx = m_xMax-m_xMin;
    double dy = m_yMax-m_yMin;
    QVector<QLineF> sepLines;

    for(auto const& p: m_data) {
        // y = INF ?
        if(p.second > std::numeric_limits<qreal>::max() ) {
            // Horizontal line
            x = plotFrame.x()+(p.first - m_xMin)/dx*plotFrame.width();

            sepLines.append(QLineF(x,plotFrame.y(),x,plotFrame.y()+plotFrame.height()));
            lastPoint.setX(0);
            lastPoint.setY(0);
            isOutside = false;
            skippedFirst = false;
            continue;
        }

        x = plotFrame.x()+(p.first - m_xMin)/dx*plotFrame.width();
        y = plotFrame.y()+(1-(p.second - m_yMin)/dy)*plotFrame.height(); // Flip y axis

        QPointF newPoint;
        // Line is inside ?
        if(p.first >= m_xMin && p.first <= m_xMax &&
                p.second >= m_yMin && p.second <= m_yMax) {
            // Clipping with reentering line
            if(isOutside) {
                lastPoint.setX(qMax(plotFrame.x(),qMin(plotFrameLowRight.x(),lastPoint.x())));
                if(!isnanf(lastPoint.y()))
                    lastPoint.setY(qMax(plotFrame.y(),qMin(plotFrameLowRight.y(),lastPoint.y())));
                else
                    lastPoint.setY(y);
                isOutside = false;
            }
            newPoint.setX(x);
            newPoint.setY(y);

            if(skippedFirst && !isnanf(newPoint.y()) && !isnanf(lastPoint.y()))
                lines.append(QLineF(lastPoint,newPoint));
            skippedFirst = true;

        }
        // Clip line, that comes from inside and goes to the outside ?
        else if(!isOutside) {
            // Clip leaving line
            newPoint.setX(qMax(plotFrame.x(),qMin(plotFrameLowRight.x(),x)));
            if(!isnanf(y))
                newPoint.setY(qMax(plotFrame.y(),qMin(plotFrameLowRight.y(),y)));
            else
                newPoint.setY(y);
            isOutside = true;

            if(skippedFirst && !isnanf(newPoint.y()) && !isnanf(lastPoint.y()))
                lines.append(QLineF(lastPoint,newPoint));
            skippedFirst = true;
        } // Store point for next run only
        else {
            newPoint.setX(x);
            newPoint.setY(y);
        }

        lastPoint = newPoint;
    }

    painter.drawLines(lines);
    painter.setPen(QPen(Qt::blue,2));
    painter.drawLines(sepLines);

    // Print value next to line end
    QFontMetrics fm = painter.fontMetrics();
    if(!isnanf(m_data.rbegin()->second)) {
        QString val = QString("%1").arg(m_data.rbegin()->second);
        x = lastPoint.x()-fm.width(val)-1;
        // Keep y in range of plot
        y = qMin(plotFrame.y()+plotFrame.height(),qMax(plotFrame.y()+fm.height(),lastPoint.y()-5));
        painter.drawText(x,y,val);
    }

}
QRect SimpleTimePlot::drawFrame(QPainter *painter)
{
    int borderX = 50;
    int borderY = 20;
    QRect plotFrame(borderX,borderY,qMax(0,width()-2*borderX),qMax(0,height()-2*borderY));
    painter->setPen(QPen(Qt::red));
    //painter->drawRect(0,0,width()-1,height()-1);
    painter->drawRect(plotFrame);

    painter->setPen(QPen(Qt::black));
    QFontMetrics fm = painter->fontMetrics();
    int fontHeight = fm.height();
    QString xMin = QString("%1").arg(m_xMin);
    QString xMax = QString("%1").arg(m_xMax);
    QString yMin = QString("%1").arg(m_yMin);
    QString yMax = QString("%1").arg(m_yMax);
    painter->drawText(plotFrame.x(),fontHeight+plotFrame.y()+plotFrame.height(),xMin);
    painter->drawText(plotFrame.x()+ plotFrame.width() - fm.width(xMax),fontHeight+plotFrame.y()+plotFrame.height(),xMax);
    painter->drawText(borderX-1-fm.width(yMin),plotFrame.y()+plotFrame.height(),yMin);
    painter->drawText(borderX-1-fm.width(yMax),fontHeight+plotFrame.y(),yMax);

    // Draw header
    painter->drawText(borderX,fontHeight, m_tite);
    return plotFrame;
}
void SimpleTimePlot::cleanupMap()
{
    auto itEnd = m_data.lower_bound(m_xMin);
    if(itEnd == m_data.end())
        return;
    // Check if we are the first element in the list
    // If yes, do nothing
    int dist = std::distance(m_data.begin(),itEnd);
    if(dist <= 0)
        return;

    // Keep the found element for clipping
    // Only delete previous elements
    itEnd--;

    // Erase all old values
    while(m_data.begin() != itEnd)
        m_data.erase(m_data.begin());
}
