#include "aspectratiopixmap.h"

AspectRatioPixmap::AspectRatioPixmap(QWidget *parent) :
    QLabel(parent)
{
    this->setMinimumSize(1,1);
    setScaledContents(false);
}

void AspectRatioPixmap::setPixmap ( const QPixmap & p)
{
    pix = p;
    QLabel::setPixmap(scaledPixmap());
}

int AspectRatioPixmap::heightForWidth( int width ) const
{
    return pix.isNull() ? this->height() : ((qreal)pix.height()*width)/pix.width();
}

QSize AspectRatioPixmap::sizeHint() const
{
    int w = this->width();
    return QSize( w, heightForWidth(w) );
}

QPixmap AspectRatioPixmap::scaledPixmap() const
{
    if(!pix.isNull())
        return pix.scaled(this->size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    else
        return pix;
}

void AspectRatioPixmap::resizeEvent(QResizeEvent * e)
{
    if(!pix.isNull())
        QLabel::setPixmap(scaledPixmap());
}
