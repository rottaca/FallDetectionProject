#ifndef ASPECTRATIOPIXMAP_H
#define ASPECTRATIOPIXMAP_H

#include <QLabel>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QResizeEvent>


class AspectRatioPixmap : public QLabel
{
    Q_OBJECT
public:
    explicit AspectRatioPixmap(QWidget *parent = 0);
    virtual int heightForWidth( int width ) const;
    virtual QSize sizeHint() const;
    QPixmap scaledPixmap() const;

public slots:
    void setPixmap ( const QPixmap & );
    void resizeEvent(QResizeEvent *);
    void clear()
    {
        setPixmap(QPixmap());
    }

private:
    QPixmap pix;
};

#endif // ASPECTRATIOPIXMAP_H
