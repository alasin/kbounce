/*
 * Copyright (C) 2000 Stefan Schimanski <1Stein@gmx.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License,Life or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <qlayout.h>
#include <klocale.h>
#include <kapplication.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kstdgameaction.h>
#include <qtimer.h>
#include <qlcdnumber.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <kscoredialog.h>
#include <kstatusbar.h>

#include "kbounce.h"
#include "game.h"
#include <qlabel.h>

KJezzball::KJezzball()
    : KMainWindow(0), m_gameWidget( 0 )
{
    // setup variables
    m_game.level = 1;
    m_game.score = 0;
    m_state = Idle;

    KConfig *config = kapp->config();
    m_backgroundDir = config->readPathEntry( "BackgroundDir" );
    m_showBackground = config->readBoolEntry( "ShowBackground", false );

    initXMLUI();
    statusBar();

    // create widgets
    m_view = new QWidget( this, "m_view" );
    setCentralWidget( m_view );

    m_layout = new QGridLayout( m_view, 1, 3 );
    m_layout->setColStretch( 2, 1 );

    QVBoxLayout *infoLayout = new QVBoxLayout;
    m_layout->addLayout( infoLayout, 0, 1 );

    QLabel *label = new QLabel( i18n("Level:"), m_view );
    infoLayout->addWidget( label );
    m_levelLCD = new QLCDNumber( 5, m_view );
    infoLayout->addWidget( m_levelLCD );

    label = new QLabel( i18n("Score:"), m_view );
    infoLayout->addWidget( label );
    m_scoreLCD = new QLCDNumber( 5, m_view );
    infoLayout->addWidget( m_scoreLCD );

    infoLayout->addSpacing( 20 );

    label = new QLabel( i18n("Filled area:"), m_view );
    infoLayout->addWidget( label );
    m_percentLCD  = new QLCDNumber( 5, m_view );
    infoLayout->addWidget( m_percentLCD );

    label = new QLabel( i18n("Lives:"), m_view );
    infoLayout->addWidget( label );
    m_lifesLCD  = new QLCDNumber( 5, m_view );
    infoLayout->addWidget( m_lifesLCD );

    label = new QLabel( i18n("Time:"), m_view );
    infoLayout->addWidget( label );
    m_timeLCD  = new QLCDNumber( 5, m_view );
    infoLayout->addWidget( m_timeLCD );

    // create timers
    m_nextLevelTimer = new QTimer( this, "m_nextLevelTimer" );
    connect( m_nextLevelTimer, SIGNAL(timeout()), this, SLOT(switchLevel()) );

    m_gameOverTimer = new QTimer( this, "m_gameOverTimer" );
    connect( m_gameOverTimer, SIGNAL(timeout()), this, SLOT(gameOverNow()) );

    m_timer = new QTimer( this, "m_timer" );
    connect( m_timer, SIGNAL(timeout()), this, SLOT(second()) );

    // create demo game
    createLevel( 1 );
    statusBar()->message( i18n("Press <Space> to start a game!") );
    //m_gameWidget->display( i18n("Press <Space> to start a game!") );
}

/**
 * create the action events create the gui. 
 */
void KJezzball::initXMLUI()
{
    KStdGameAction::gameNew( this, SLOT(newGame()), actionCollection() );
    KStdGameAction::quit( this, SLOT(close()), actionCollection() );
    KStdGameAction::highscores(this, SLOT(showHighscore()), actionCollection() );
    KStdGameAction::pause(this, SLOT(pauseGame()), actionCollection());
    KStdGameAction::end(this, SLOT(closeGame()), actionCollection());

    (void)new KAction( i18n("&Select Image Directory..."), 0, this, SLOT(selectBackground()),
                       actionCollection(), "background_select" );
    KToggleAction *show = new KToggleAction( i18n("Show &Images"), 0, this, SLOT(showBackground()), actionCollection(), "background_show" );

    show->setEnabled( !m_backgroundDir.isEmpty() );
    show->setChecked( m_showBackground );

    createGUI( "kbounceui.rc" );
}

void KJezzball::newGame()
{
    // Check for running game
    closeGame();
    if ( m_state==Idle )
    {
        // update displays
        m_game.level = 1;
        m_game.score = 0;

        m_levelLCD->display( m_game.level );
        m_scoreLCD->display( m_game.score );

        statusBar()->clear();
        
        // start new game
        m_state = Running;

        createLevel( m_game.level );
        startLevel();
    }
}

void KJezzball::closeGame()
{
    if ( m_state!=Idle )
    {
        int ret = KMessageBox::questionYesNo( this, i18n("Do you really want to close the running game?") );
        if ( ret==KMessageBox::Yes )
        {
            stopLevel();
            m_state = Idle;
            highscore();
        }
    }
}

void KJezzball::pauseGame()
{
    switch ( m_state )
    {
    case Running:
        m_state = Paused;
        statusBar()->message(i18n("Game paused.") );
        //m_gameWidget->display( i18n("Game paused. Press P to continue!") );
        stopLevel();
        break;

    case Paused:
    case Suspend:
        m_state = Running;
        statusBar()->clear();
        //m_gameWidget->display( QString::null );
        startLevel();
        break;

    case Idle:
        break;
    }
}

void KJezzball::gameOver()
{
    stopLevel();
    m_gameOverTimer->start( 100, TRUE );
}


void KJezzball::gameOverNow()
{
    m_state = Idle;

    QString score;
    score.setNum( m_game.score );
    KMessageBox::information( this, i18n("Game Over! Score: %1").arg(score) );
    statusBar()->message(  i18n("Game over. Press <Space> for a new game") );
    //m_gameWidget->display( i18n("Game over. Press <Space> for a new game!") );
    highscore();
}

/**
 * Bring up the standard kde high score dialog.
 */
void KJezzball::showHighscore(){
    KScoreDialog h(KScoreDialog::Name | KScoreDialog::Level | KScoreDialog::Score, this);
    h.exec();
}

/**
 * Select a background image.
 */
void KJezzball::selectBackground()
{
    QString path = KFileDialog::getExistingDirectory( m_backgroundDir,  this,
                                                      i18n("Select Background Image Directory") );
    if ( !path.isEmpty() && path!=m_backgroundDir ) {
        m_backgroundDir = path;

        // enable action
        KAction *a = actionCollection()->action("background_show");
        if ( a ) a->setEnabled(true);

        // save settings
        KConfig *config = kapp->config();
        config->writeEntry( "BackgroundDir", m_backgroundDir );
        config->sync();

        // apply background setting
        if ( m_showBackground ) {
            if ( m_background.width()==0 )
                m_background = getBackgroundPixmap();

            m_gameWidget->setBackground( m_background );
        }
        else{
          KMessageBox::information( this, i18n("You may now turn on background images."));
        }
    }
}

void KJezzball::showBackground()
{
    KToggleAction *a = dynamic_cast<KToggleAction*>(actionCollection()->action("background_show"));
    if( a ) {

        bool show = a->isChecked();
        if ( show!=m_showBackground ) {

            m_showBackground = show;

            // save setting
            KConfig *config = kapp->config();
            config->writeEntry( "ShowBackground", m_showBackground );
            config->sync();

            // update field
            if ( m_showBackground ) {
                if ( m_background.width()==0 )
                    m_background = getBackgroundPixmap();
            }

            m_gameWidget->setBackground( m_showBackground ? m_background : QPixmap() );
        }

    }
}

QPixmap KJezzball::getBackgroundPixmap()
{
    // list directory
    QDir dir( m_backgroundDir, "*.png *.jpg", QDir::Name|QDir::IgnoreCase, QDir::Files );
    if ( !dir.exists() ) {
        kdDebug(1433) << "Directory not found" << endl;
        return QPixmap();
    }

    // return random pixmap
    int num = rand() % dir.count();
    return QPixmap( dir.absFilePath( dir[num] ) );
}

void KJezzball::focusOutEvent( QFocusEvent *ev )
{
    if ( m_state==Running )
    {
        stopLevel();
        m_state = Suspend;
        statusBar()->message( i18n("Game suspended") );
        // m_gameWidget->display( i18n("Game suspended") );
    }

    KMainWindow::focusOutEvent( ev );
}

void KJezzball::focusInEvent ( QFocusEvent *ev )
{
    if ( m_state==Suspend )
    {
        startLevel();
        m_state = Running;
        statusBar()->clear();
        //m_gameWidget->display( QString::null );
    }

    KMainWindow::focusInEvent( ev );
}

void KJezzball::keyPressEvent( QKeyEvent *ev )
{
   if ((m_state==Idle) && (ev->key() == Key_Space))
   {
      ev->accept();
      newGame();
      return;
   }
   KMainWindow::keyPressEvent( ev );
}


void KJezzball::second()
{
    m_level.time--;
    m_timeLCD->display( m_level.time );
    if ( m_level.time<=0 )
    {
        JezzGame::playSound( "timeout.au" );
        gameOver();
    } else
        if ( m_level.time<=30 )
            JezzGame::playSound( "seconds.au" );
}

void KJezzball::died()
{
    kdDebug() << "died" << endl;
    m_level.lifes--;
    m_lifesLCD->display( m_level.lifes );
    if ( m_level.lifes==0 ) gameOver();
}

void KJezzball::newPercent( int percent )
{
    m_percentLCD->display( percent );
    if ( percent>=75 )
    {
        m_level.score = m_level.lifes*15 + (percent-75)*2*(m_game.level+5);
        nextLevel();
    }
}

void KJezzball::createLevel( int level )
{
    // destroy old game
    if ( m_gameWidget ) delete m_gameWidget;

    // create new game widget
    if ( m_showBackground )
        m_background = getBackgroundPixmap();
    else
        m_background = QPixmap();

    m_gameWidget = new JezzGame( m_background, level+1, m_view, "m_gameWidget" );

    m_gameWidget->show();
    m_layout->addWidget( m_gameWidget, 0, 0 );
    connect( m_gameWidget, SIGNAL(died()), this, SLOT(died()) );
    connect( m_gameWidget, SIGNAL(newPercent(int)), this, SLOT(newPercent(int)) );

    // update displays
    m_level.lifes = level+1;
    m_lifesLCD->display( m_level.lifes );
    m_percentLCD->display( 0 );

    m_level.time = (level+2)*30;
    m_timeLCD->display( m_level.time );

    m_level.score = 0;
}

void KJezzball::startLevel()
{
    if ( m_gameWidget )
    {
        m_timer->start( 1000 );
        m_gameWidget->start();
    }
}

void KJezzball::stopLevel()
{
    if ( m_gameWidget )
    {
        m_gameWidget->stop();
        m_timer->stop();
    }
}

void KJezzball::nextLevel()
{
    stopLevel();
    m_nextLevelTimer->start( 100, TRUE );
}

void KJezzball::switchLevel()
{
    m_game.score += m_level.score;
    m_scoreLCD->display( m_game.score );

    QString score;
    score.setNum( m_level.score );

    QString level;
    level.setNum( m_game.level );

QString foo = QString(
i18n("You have successfully cleared more then 75% of the board.") +
"\n%1 points : %2\n"
"%3 points : %4\n"
"%5 points : %6\n"
+ i18n("On to level %7, remember you get %8 lives this time!")).arg(
m_level.lifes*15).arg(i18n("15 points per remaining life")).arg(
(m_gameWidget->percent()-75)*2*(m_game.level+5)).arg(i18n("Bonus")).arg(
score).arg(i18n("Total score for this level")).arg(
m_game.level+1).arg(m_game.level+2);


   KMessageBox::information( this,foo );


   // KMessageBox::information( this, i18n("You've completed level %1 with "
   //     "a score of %2.\nGet ready for the next one!").arg(level).arg(score));

    m_game.level++;
    m_levelLCD->display( m_game.level );

    createLevel( m_game.level );
    startLevel();
}


void KJezzball::highscore()
{
    KScoreDialog d(KScoreDialog::Name | KScoreDialog::Level | KScoreDialog::Score, this);

    KScoreDialog::FieldInfo scoreInfo;
    
    scoreInfo[KScoreDialog::Level].setNum(m_game.level);

    if (!d.addScore(m_game.score, scoreInfo))
       return;
    
    // Show highscore & ask for name.
    d.exec();
}

#include "kbounce.moc"