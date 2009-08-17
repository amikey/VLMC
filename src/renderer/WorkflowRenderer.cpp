/*****************************************************************************
 * WorkflowRenderer.cpp: Allow a current workflow preview
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

#include <QtDebug>
#include <QThread>

#include "vlmc.h"
#include "WorkflowRenderer.h"
#include "Timeline.h"

WorkflowRenderer::WorkflowRenderer( MainWorkflow* mainWorkflow ) :
            m_mainWorkflow( mainWorkflow ),
            m_pauseAsked( false ),
            m_unpauseAsked( false ),
            m_stopping( false )
{
    char        buffer[64];

    m_actionsLock = new QReadWriteLock;
    m_media = new LibVLCpp::Media( "fake://" );

    sprintf( buffer, ":invmem-width=%i", VIDEOWIDTH );
    m_media->addOption( ":codec=invmem" );
    m_media->addOption( buffer );
    sprintf( buffer, ":invmem-height=%i", VIDEOHEIGHT );
    m_media->addOption( buffer );
    sprintf( buffer, ":invmem-lock=%lld", (qint64)WorkflowRenderer::lock );
    m_media->addOption( buffer );
    sprintf( buffer, ":invmem-unlock=%lld", (qint64)WorkflowRenderer::unlock );
    m_media->addOption( buffer );
    sprintf( buffer, ":invmem-data=%lld", (qint64)this );
    m_media->addOption( buffer );
    sprintf( buffer, ":width=%i", VIDEOWIDTH );
    m_media->addOption( buffer );
    sprintf( buffer, ":height=%i", VIDEOHEIGHT );
    m_media->addOption( buffer );

    m_mediaPlayer->setMedia( m_media );

    connect( m_mediaPlayer, SIGNAL( playing() ),    this,   SLOT( __videoPlaying() ), Qt::DirectConnection );
    connect( m_mediaPlayer, SIGNAL( paused() ),     this,   SLOT( __videoPaused() ), Qt::DirectConnection );
    connect( m_mediaPlayer, SIGNAL( stopped() ),    this,   SLOT( __videoStopped() ) );
    connect( m_mainWorkflow, SIGNAL( mainWorkflowEndReached() ), this, SLOT( __endReached() ) );
    connect( m_mainWorkflow, SIGNAL( positionChanged( float ) ), this, SLOT( __positionChanged( float ) ) );

    m_condMutex = new QMutex;
    m_waitCond = new QWaitCondition;
}


WorkflowRenderer::~WorkflowRenderer()
{
    stop();

    disconnect( m_mediaPlayer, SIGNAL( playing() ),    this,   SLOT( __videoPlaying() ) );
    disconnect( m_mediaPlayer, SIGNAL( paused() ),     this,   SLOT( __videoPaused() ) );
    disconnect( m_mediaPlayer, SIGNAL( stopped() ),    this,   SLOT( __videoStopped() ) );
    disconnect( m_mainWorkflow, SIGNAL( mainWorkflowEndReached() ), this, SLOT( __endReached() ) );
    disconnect( m_mainWorkflow, SIGNAL( positionChanged( float ) ), this, SLOT( __positionChanged( float ) ) );

    delete m_actionsLock;
    delete m_media;
    delete m_condMutex;
    delete m_waitCond;
}

void*   WorkflowRenderer::lock( void* datas )
{
    WorkflowRenderer* self = reinterpret_cast<WorkflowRenderer*>( datas );

    if ( self->m_stopping == false )
    {
        void* ret = self->m_mainWorkflow->getSynchroneOutput();
        self->m_lastFrame = static_cast<unsigned char*>( ret );
        return ret;
    }
    return self->m_lastFrame;
}

void    WorkflowRenderer::unlock( void* datas )
{
    WorkflowRenderer* self = reinterpret_cast<WorkflowRenderer*>( datas );
    self->checkActions();
}

void        WorkflowRenderer::checkActions()
{
    QReadLocker     lock( m_actionsLock );

    if ( m_actions.size() == 0 )
        return ;
    while ( m_actions.empty() == false )
    {
        Actions act = m_actions.top();
        m_actions.pop();
        switch ( act )
        {
            case    Pause:
                if ( m_pauseAsked == true )
                    continue ;
                m_pauseAsked = true;
//                m_mediaPlayer->pause();
                pauseMainWorkflow();
                //This will also pause the MainWorkflow via a signal/slot
                break ;
            default:
                qDebug() << "Unhandled action:" << act;
                break ;
        }
    }
}

void        WorkflowRenderer::stopPreview()
{
    disconnect( m_mainWorkflow, SIGNAL( frameChanged(qint64) ),
             Timeline::getInstance()->tracksView()->tracksCursor(), SLOT( updateCursorPos( qint64 ) ) );
    stop();
}

void        WorkflowRenderer::startPreview()
{
    char        buff[128];

    connect( m_mainWorkflow, SIGNAL( frameChanged(qint64) ),
            Timeline::getInstance()->tracksView()->tracksCursor(), SLOT( updateCursorPos( qint64 ) ) );
    connect( m_mainWorkflow, SIGNAL( mainWorkflowPaused() ), this, SLOT( mainWorkflowPaused() ) );
    connect( m_mainWorkflow, SIGNAL( mainWorkflowUnpaused() ), this, SLOT( mainWorkflowUnpaused() ) );
    m_mainWorkflow->startRender();
    sprintf( buff, ":fake-duration=%lli", m_mainWorkflow->getLength() / FPS * 1000 );
    m_media->addOption( buff );
    m_mediaPlayer->play();
    m_isRendering = true;
    m_paused = false;
    m_stopping = false;
    m_lastFrame = NULL;
}

void        WorkflowRenderer::setPosition( float newPos )
{
    m_mainWorkflow->setPosition( newPos );
}

void        WorkflowRenderer::nextFrame()
{
    m_mainWorkflow->nextFrame();
}

void        WorkflowRenderer::previousFrame()
{
    m_mainWorkflow->previousFrame();
}

void        WorkflowRenderer::pauseMainWorkflow()
{
    if ( m_paused == true )
        return ;

    QMutexLocker    lock( m_condMutex );
    m_mainWorkflow->pause();
    m_waitCond->wait( m_condMutex );
}

void        WorkflowRenderer::unpauseMainWorkflow()
{
    if ( m_paused == false )
        return ;
    m_mainWorkflow->unpause();
}

void        WorkflowRenderer::mainWorkflowPaused()
{
    m_paused = true;
    m_pauseAsked = false;
    {
        QMutexLocker    lock( m_condMutex );
    }
    m_waitCond->wakeAll();
    emit paused();
}

void        WorkflowRenderer::mainWorkflowUnpaused()
{
    m_paused = false;
    m_unpauseAsked = false;
    emit playing();
}

void        WorkflowRenderer::togglePlayPause( bool forcePause )
{
    if ( m_isRendering == false && forcePause == false )
        startPreview();
    else
        internalPlayPause( forcePause );
}

void        WorkflowRenderer::internalPlayPause( bool forcePause )
{
    //If force pause is true, we just ensure that this render is paused... no need to start it.
    if ( m_isRendering == true )
    {
        if ( m_paused == true && forcePause == false )
        {
            if ( m_paused == true )
            {
                m_unpauseAsked = true;
                unpauseMainWorkflow();
            }
        }
        else
        {
            if ( m_paused == false )
            {
                QWriteLocker        lock( m_actionsLock );
                m_actions.push( Pause );
            }
        }
    }
}

void        WorkflowRenderer::stop()
{
    m_isRendering = false;
    m_paused = false;
    m_pauseAsked = false;
    m_unpauseAsked = false;
    m_stopping = true;
    m_mainWorkflow->cancelSynchronisation();
    m_mediaPlayer->stop();
    m_mainWorkflow->stop();
}

/////////////////////////////////////////////////////////////////////
/////SLOTS :
/////////////////////////////////////////////////////////////////////

void        WorkflowRenderer::__endReached()
{
    stopPreview();
    emit endReached();
}

void        WorkflowRenderer::__positionChanged()
{
    qFatal("This should never be used ! Get out of here !");
}

void        WorkflowRenderer::__positionChanged( float pos )
{
    emit positionChanged( pos );
}

void        WorkflowRenderer::__videoPaused()
{
    if ( m_pauseAsked == true )
        pauseMainWorkflow();
}

void        WorkflowRenderer::__videoPlaying()
{
    if ( m_unpauseAsked == true )
        unpauseMainWorkflow();
    else
    {
        m_paused = false;
        emit playing();
    }
}

void        WorkflowRenderer::__videoStopped()
{
    emit endReached();
}
