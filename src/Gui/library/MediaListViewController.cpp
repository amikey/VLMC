/*****************************************************************************
 * MediaLibraryWidget.h
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Thomas Boquet <thomas.boquet@gmail.com>
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

#include "MediaListViewController.h"

#include <QDebug>

MediaListViewController::MediaListViewController( StackViewController* nav ) :
        ListViewController( nav ), m_nav( nav ), m_clipsListView( 0 )
{
    connect( Library::getInstance(), SIGNAL( newMediaLoaded( Media* ) ),
             this, SLOT( newMediaLoaded( Media* ) ) );
    m_cells = new QHash<QUuid, QWidget*>();
    connect( m_nav, SIGNAL( previousButtonPushed() ), this, SLOT( restoreContext() ) );
}

MediaListViewController::~MediaListViewController()
{
    delete m_cells;
}

void        MediaListViewController::newMediaLoaded( Media* media )
{
    MediaCellView* cell = new MediaCellView( media->uuid() );

    connect( cell, SIGNAL ( cellSelected( QUuid ) ),
             this, SLOT ( cellSelection( QUuid ) ) );
    connect( cell, SIGNAL ( cellDeleted( QUuid ) ),
             this, SIGNAL( mediaDeleted( QUuid ) ) );
    connect( cell, SIGNAL( arrowClicked( const QUuid& ) ),
             this, SLOT( showClipList( const QUuid& ) ) );
    connect( media, SIGNAL( snapshotComputed( const Media* ) ),
             this, SLOT( updateCell( const Media* ) ) );
    cell->setNbClips( media->clips()->size() );
    cell->setThumbnail( media->snapshot() );
    cell->setTitle( media->fileName() );
    cell->setLength( media->lengthMS() );
    if ( media->baseClip() != NULL )
        cell->setEnabled(true);
    addCell(cell);
    m_cells->insert( media->uuid(), cell );
}

void    MediaListViewController::cellSelection( const QUuid& uuid )
{
    if ( m_currentUuid == uuid )
        return;

    if ( m_cells->contains( uuid ) )
    {
        if ( !m_currentUuid.isNull() && m_cells->contains( m_currentUuid ) )
        {
            QWidget* cell = m_cells->value( m_currentUuid );
            cell->setPalette( m_cells->value( uuid )->palette() );
        }
        QPalette p = m_cells->value( uuid )->palette();
        p.setColor( QPalette::Window, QColor( Qt::darkBlue ) );
        m_cells->value( uuid )->setPalette( p );
        m_currentUuid = uuid;
        emit mediaSelected( Library::getInstance()->media( uuid ) );
    }
}

void    MediaListViewController::mediaRemoved( const QUuid& uuid )
{
    QWidget* cell = m_cells->value( uuid );
    removeCell( cell );
    m_cells->remove( uuid );
    m_currentUuid = QUuid();
}

void    MediaListViewController::updateCell( const Media* media )
{
    MediaCellView* cell = qobject_cast<MediaCellView*>( m_cells->value( media->uuid(), NULL ) );
    if ( cell != NULL )
    {
        cell->setNbClips( media->clips()->size() );
        cell->setLength( media->lengthMS() );
        cell->setThumbnail( media->snapshot() );
        cell->setEnabled(true);
    }
}

void    MediaListViewController::showClipList( const QUuid& uuid )
{
    if ( !m_cells->contains( uuid ) )
        return ;
    if ( Library::getInstance()->media( uuid ) == NULL ||
         Library::getInstance()->media( uuid )->clips()->size() == 0 )
        return ;
    m_lastUuidClipListAsked = uuid;
    m_clipsListView = new ClipListViewController( m_nav, uuid );
    m_clipsListView->addClipsFromMedia( Library::getInstance()->media( uuid ) );
    connect( m_clipsListView, SIGNAL( clipSelected( const QUuid& ) ),
            this, SLOT( clipSelection( const QUuid& ) ) );
    m_nav->pushViewController( m_clipsListView );
}

void    MediaListViewController::newClipAdded( Clip* clip )
{
    if ( clip->getParent() == 0 )
        return ;
    const QUuid& uuid = clip->getParent()->uuid();

    if ( m_cells->contains( uuid ) )
    {
        MediaCellView*  cell = qobject_cast<MediaCellView*>( m_cells->value( uuid, 0 ) );
        if ( cell != 0 )
        {
            cell->incrementClipCount();
        }
    }
}

void    MediaListViewController::restoreContext()
{
    if ( m_clipsListView->getNbDeletion() != 0 )
    {
        if ( m_cells->contains( m_lastUuidClipListAsked ) )
        {
            MediaCellView*  cell = qobject_cast<MediaCellView*>( m_cells->value( m_lastUuidClipListAsked, 0 ) );
            if ( cell != 0 )
            {
                cell->decrementClipCount( m_clipsListView->getNbDeletion() );
                m_clipsListView->resetNbDeletion();
            }
        }
    }
    delete m_clipsListView;
}

void
MediaListViewController::clipSelection( const QUuid& uuid )
{
    Clip* clip;
    if ( ( clip = Library::getInstance()->clip( m_currentUuid, uuid ) ) != 0 )
    {
        emit clipSelected( clip );
    }
}
