/*****************************************************************************
 * RenderPreviewWidget.h: Allow a current workflow preview
 *****************************************************************************
 * Copyright (C) 2008-2009 the VLMC team
 *
 * Authors: Hugo Beauzee-Luyssen <hugo@vlmc.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef RENDERPREVIEWWIDGET_H
#define RENDERPREVIEWWIDGET_H

#include <QObject>
#include <QWidget>
#include <QtDebug>

#include "VLCMediaPlayer.h"
#include "Workflow/MainWorkflow.h"

//TODO: This should really share a common interface with ClipPreviewWorkflow
class   RenderPreviewWidget : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY( RenderPreviewWidget )

    public:
        RenderPreviewWidget( MainWorkflow* mainWorkflow, QWidget* renderWidget );
        ~RenderPreviewWidget();

        void                        startPreview();
        void                        setPosition( float newPos );
        void                        togglePlayPause();

    private:
        MainWorkflow*               m_mainWorkflow;
        LibVLCpp::MediaPlayer*      m_mediaPlayer;

public slots:
    void                    __positionChanged();
    void                    __videoPaused();
    void                    __videoPlaying();
    void                    __endReached();

signals:
    void                    stopped();
    void                    paused();
    void                    playing();
    void                    positionChanged( float );
    void                    endReached();
};

#endif // RENDERPREVIEWWIDGET_H