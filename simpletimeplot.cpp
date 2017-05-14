#include "simpletimeplot.h"

#include <QPainter>

SimpleTimePlot::SimpleTimePlot(QWidget *parent) : QWidget(parent)
{

}

void SimpleTimePlot::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    QRect plotFrame = drawFrame(&painter);

    painter.setPen(QPen(Qt::black));

    QVector<QLineF> lines;
    QPointF lastPoint(plotFrame.x(),plotFrame.y()+plotFrame.height());
    bool isOutside = false;
    for(auto const& p: m_data) {
        float x,y;
        x = plotFrame.x()+(p.first - m_xMin)/(m_xMax-m_xMin)*plotFrame.width();
        y = plotFrame.y()+(1-(p.second - m_yMin)/(m_yMax-m_yMin))*plotFrame.height(); // Flip y axis

        QPointF newPoint;
        if(p.first >= m_xMin && p.first <= m_xMax &&
                p.second >= m_yMin && p.second <= m_yMax) {
            // Clipping with reentering line
            if(isOutside) {
                lastPoint.setX(qMax(plotFrame.x(),qMin(plotFrame.x()+plotFrame.width(),(int)lastPoint.x())));
                lastPoint.setY(qMax(plotFrame.y(),qMin(plotFrame.y()+plotFrame.height(),(int)lastPoint.y())));
                isOutside = false;
            }
            newPoint.setX(x);
            newPoint.setY(y);
            lines.append(QLineF(lastPoint,newPoint));
        } else if(!isOutside) {
            // Clip leaving line
            newPoint.setX(qMax(plotFrame.x(),qMin(plotFrame.x()+plotFrame.width(),(int)x)));
            newPoint.setY(qMax(plotFrame.y(),qMin(plotFrame.y()+plotFrame.height(),(int)y)));
            isOutside = true;
            lines.append(QLineF(lastPoint,newPoint));
        } else {
            newPoint.setX(x);
            newPoint.setY(y);
        }

        lastPoint = newPoint;
    }

    painter.drawLines(lines);
}
QRect SimpleTimePlot::drawFrame(QPainter *painter)
{
    int borderX = 30;
    int borderY = 20;
    QRect plotFrame(borderX,borderY,width()-2*borderX,height()-2*borderY);
    painter->setPen(QPen(Qt::red));
    painter->drawRect(0,0,width()-1,height()-1);
    painter->drawRect(plotFrame);

    painter->setPen(QPen(Qt::black));
    QFontMetrics fm = painter->fontMetrics();
    QString xMin = QString("%1").arg(m_xMin);
    QString xMax = QString("%1").arg(m_xMax);
    QString yMin = QString("%1").arg(m_yMin);
    QString yMax = QString("%1").arg(m_yMax);
    painter->drawText(plotFrame.x(),15+plotFrame.y()+plotFrame.height(),xMin);
    painter->drawText(plotFrame.x()+ plotFrame.width() - fm.width(xMax),15+plotFrame.y()+plotFrame.height(),xMax);
    painter->drawText(borderX-1-fm.width(yMin),plotFrame.y()+plotFrame.height(),yMin);
    painter->drawText(borderX-1-fm.width(yMax),15+plotFrame.y(),yMax);
    return plotFrame;
}
