/****************************************************************************
* VLC-Qt - Qt and libvlc connector library
* Copyright (C) 2013 Tadej Novak <tadej@tano.si>
*
* This library is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this library. If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include <QtCore/QDebug>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>

#include <vlc/vlc.h>

#include "core/Audio.h"
#include "core/Common.h"
#include "core/Instance.h"
#include "core/Media.h"
#include "core/MediaPlayer.h"

#if QT_VERSION >= 0x050000
    #include "qml/QmlVideoPlayer5.h"
#else
    #include "qml/QmlVideoPlayer4.h"
#endif

// VLC display functions
static void display(void *core,
                    void *picture)
{
    (void) core;

    Q_ASSERT(picture == NULL);
}

static void *lock(void *core,
                  void **planes)
{
    VlcQmlVideoPlayer *player = (VlcQmlVideoPlayer *)core;
    player->_mutex.lock();
    *planes = player->_frame->bits();
    return NULL;
}

static void unlock(void *core,
                   void *picture,
                   void *const *planes)
{
    Q_UNUSED(planes)

    VlcQmlVideoPlayer *player = (VlcQmlVideoPlayer *)core;
    player->_mutex.unlock();

    Q_ASSERT(picture == NULL);
}

#if QT_VERSION >= 0x050000
VlcQmlVideoPlayer::VlcQmlVideoPlayer(QQuickItem *parent)
    : QQuickPaintedItem(parent),
#else
VlcQmlVideoPlayer::VlcQmlVideoPlayer(QDeclarativeItem *parent)
    : QDeclarativeItem(parent),
#endif
      _hasMedia(false)
{
#if QT_VERSION >= 0x050000
    setFlag(ItemHasContents, true);
#else
    setFlag(QGraphicsItem::ItemHasNoContents, false);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
#endif

    _instance = new VlcInstance(VlcCommon::args(), this);
    _player = new VlcMediaPlayer(_instance);
    _audioManager = new VlcAudio(_player);

    _frame = new QImage(800, 600, QImage::Format_ARGB32_Premultiplied);

    _timer = new QTimer(this);
    connect(_timer,SIGNAL(timeout()),this,SLOT(updateFrame()));
    _timer->start(40); // FPS: 25
}

VlcQmlVideoPlayer::~VlcQmlVideoPlayer()
{
    _player->stop();

    delete _frame;
    delete _timer;

    delete _audioManager;
    delete _media;
    delete _player;
    delete _instance;
}

void VlcQmlVideoPlayer::geometryChanged(const QRectF &newGeometry,
                                        const QRectF &oldGeometry)
{
    if (newGeometry == oldGeometry)    {
#if QT_VERSION >= 0x050000
        QQuickPaintedItem::geometryChanged(newGeometry, oldGeometry);
#else
        QDeclarativeItem::geometryChanged(newGeometry, oldGeometry);
#endif
        return;
    }

    _mutex.lock();

    delete _frame;
    _frame = new QImage(newGeometry.width(), newGeometry.height(), QImage::Format_ARGB32_Premultiplied);

    if (_hasMedia) {
        libvlc_video_set_callbacks(_player->core(), lock, unlock, display, this);
        libvlc_video_set_format(_player->core(), "RV32", _frame->width(), _frame->height(), _frame->width() * 4);
    }

    _mutex.unlock();

#if QT_VERSION >= 0x050000
    QQuickPaintedItem::geometryChanged(newGeometry, oldGeometry);
#else
    QDeclarativeItem::geometryChanged(newGeometry, oldGeometry);
#endif
}

#if QT_VERSION >= 0x050000
void VlcQmlVideoPlayer::paint(QPainter *painter)
#else
void VlcQmlVideoPlayer::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *widget)
#endif
{
#if QT_VERSION < 0x050000
    Q_UNUSED(option)
    Q_UNUSED(widget)
#endif

    _mutex.lock();
    painter->drawImage(0, 0, *_frame);
    _mutex.unlock();
}

void VlcQmlVideoPlayer::close()
{
    _hasMedia = false;

    _player->stop();
}

void VlcQmlVideoPlayer::openFile(const QString &file)
{
    if (_media)
        delete _media;

    _media = new VlcMedia(file, true, _instance);

    openInternal();
}

void VlcQmlVideoPlayer::openStream(const QString &stream)
{
    if (_media)
        delete _media;

    _media = new VlcMedia(stream, false, _instance);

    openInternal();
}

void VlcQmlVideoPlayer::openInternal()
{
    _player->open(_media);

    libvlc_video_set_callbacks(_player->core(), lock, unlock, display, this);
    libvlc_video_set_format(_player->core(), "RV32", _frame->width(), _frame->height(), _frame->width() * 4);

    _hasMedia = true;
}

void VlcQmlVideoPlayer::pause()
{
    _player->pause();
}

void VlcQmlVideoPlayer::play()
{
    _player->play();
}

void VlcQmlVideoPlayer::stop()
{
    _player->stop();
}

void VlcQmlVideoPlayer::updateFrame()
{
    //Fix for cpu usage of calling update inside unlock.
#if QT_VERSION >= 0x050000
    update(boundingRect().toAlignedRect());
#else
    update(boundingRect());
#endif
}
