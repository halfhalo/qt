/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Qt Software Information (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the either Technology Preview License Agreement or the
** Beta Release License Agreement.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this
** package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qlineedit.h"
#include "qlineedit_p.h"

#ifndef QT_NO_LINEEDIT

#include "qabstractitemview.h"
#include "qclipboard.h"
#ifndef QT_NO_ACCESSIBILITY
#include "qaccessible.h"
#endif
#ifndef QT_NO_IM
#include "qinputcontext.h"
#include "qlist.h"
#endif

QT_BEGIN_NAMESPACE

const int QLineEditPrivate::verticalMargin(1);
const int QLineEditPrivate::horizontalMargin(2);

int QLineEditPrivate::xToPos(int x, QTextLine::CursorPosition betweenOrOn) const
{
    QRect cr = adjustedContentsRect();
    x-= cr.x() - hscroll + horizontalMargin;
    return control->xToPos(x, betweenOrOn);
}

QRect QLineEditPrivate::cursorRect() const
{
    QRect cr = adjustedContentsRect();
    int cix = cr.x() - hscroll + horizontalMargin;
    QRect crect = control->cursorRect();
    crect.moveTo(crect.topLeft() + QPoint(cix, vscroll));
    return crect;
}

#ifndef QT_NO_COMPLETER

void QLineEditPrivate::_q_completionHighlighted(QString newText)
{
    Q_Q(QLineEdit);
    if (control->completer()->completionMode() != QCompleter::InlineCompletion) {
        q->setText(newText);
    } else {
        int c = control->cursor();
        QString text = control->text();
        q->setText(text.left(c) + newText.mid(c));
        control->moveCursor(control->end(), false);
        control->moveCursor(c, true);
    }
}

#endif // QT_NO_COMPLETER

void QLineEditPrivate::_q_clipboardChanged()
{
}

void QLineEditPrivate::_q_handleWindowActivate()
{
    Q_Q(QLineEdit);
    if (!q->hasFocus() && control->hasSelectedText())
        control->deselect();
}

void QLineEditPrivate::_q_deleteSelected()
{
}

void QLineEditPrivate::_q_textEdited(const QString &text)
{
    Q_Q(QLineEdit);
#ifndef QT_NO_COMPLETER
    if (control->completer() &&
            control->completer()->completionMode() != QCompleter::InlineCompletion)
        control->complete(-1); // update the popup on cut/paste/del
#endif
    emit q->textEdited(text);
}

void QLineEditPrivate::_q_cursorPositionChanged(int from, int to)
{
    Q_Q(QLineEdit);
    q->update();
    emit q->cursorPositionChanged(from, to);
}

void QLineEditPrivate::init(const QString& txt)
{
    Q_Q(QLineEdit);
    control = new QLineControl(txt);
    QObject::connect(control, SIGNAL(textChanged(const QString &)),
            q, SIGNAL(textChanged(const QString &)));
    QObject::connect(control, SIGNAL(textEdited(const QString &)),
            q, SLOT(_q_textEdited(const QString &)));
    QObject::connect(control, SIGNAL(cursorPositionChanged(int, int)),
            q, SLOT(_q_cursorPositionChanged(int, int)));
    QObject::connect(control, SIGNAL(selectionChanged()),
            q, SIGNAL(selectionChanged()));
    QObject::connect(control, SIGNAL(accepted()),
            q, SIGNAL(returnPressed()));
    QObject::connect(control, SIGNAL(editingFinished()),
            q, SIGNAL(editingFinished()));

    // for now, going completely overboard with updates.
    QObject::connect(control, SIGNAL(selectionChanged()),
            q, SLOT(update()));

    QObject::connect(control, SIGNAL(displayTextChanged(const QString &)),
            q, SLOT(update()));
    control->setPasswordCharacter(q->style()->styleHint(QStyle::SH_LineEdit_PasswordCharacter));
#ifndef QT_NO_CURSOR
    q->setCursor(Qt::IBeamCursor);
#endif
    q->setFocusPolicy(Qt::StrongFocus);
    q->setAttribute(Qt::WA_InputMethodEnabled);
    //   Specifies that this widget can use more, but is able to survive on
    //   less, horizontal space; and is fixed vertically.
    q->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::LineEdit));
    q->setBackgroundRole(QPalette::Base);
    q->setAttribute(Qt::WA_KeyCompression);
    q->setMouseTracking(true);
    q->setAcceptDrops(true);

    q->setAttribute(Qt::WA_MacShowFocusRect);
}

QRect QLineEditPrivate::adjustedContentsRect() const
{
    Q_Q(const QLineEdit);
    QStyleOptionFrameV2 opt;
    q->initStyleOption(&opt);
    QRect r = q->style()->subElementRect(QStyle::SE_LineEditContents, &opt, q);
    r.setX(r.x() + leftTextMargin);
    r.setY(r.y() + topTextMargin);
    r.setRight(r.right() - rightTextMargin);
    r.setBottom(r.bottom() - bottomTextMargin);
    return r;
}

void QLineEditPrivate::setCursorVisible(bool visible)
{
    Q_Q(QLineEdit);
    if ((bool)cursorVisible == visible)
        return;
    cursorVisible = visible;
    QRect r = cursorRect();
    if (control->inputMask().isEmpty())
        q->update(r);
    else
        q->update();
}

void QLineEditPrivate::updatePasswordEchoEditing(bool editing)
{
    Q_Q(QLineEdit);
    control->updatePasswordEchoEditing(editing);
    q->setAttribute(Qt::WA_InputMethodEnabled, shouldEnableInputMethod());
}

/*!
  This function is not intended as polymorphic usage. Just a shared code
  fragment that calls QInputContext::mouseHandler for this
  class.
*/
bool QLineEditPrivate::sendMouseEventToInputContext( QMouseEvent *e )
{
#if !defined QT_NO_IM
    Q_Q(QLineEdit);
    if ( control->composeMode() ) {
	int tmp_cursor = xToPos(e->pos().x());
	int mousePos = tmp_cursor - control->cursor();
	if ( mousePos < 0 || mousePos > control->preeditAreaText().length() ) {
            mousePos = -1;
	    // don't send move events outside the preedit area
            if ( e->type() == QEvent::MouseMove )
                return true;
        }

        QInputContext *qic = q->inputContext();
        if ( qic )
            // may be causing reset() in some input methods
            qic->mouseHandler(mousePos, e);
        if (!control->preeditAreaText().isEmpty())
            return true;
    }
#else
    Q_UNUSED(e);
#endif

    return false;
}

#ifndef QT_NO_DRAGANDDROP
void QLineEditPrivate::drag()
{
    Q_Q(QLineEdit);
    dndTimer.stop();
    QMimeData *data = new QMimeData;
    data->setText(control->selectedText());
    QDrag *drag = new QDrag(q);
    drag->setMimeData(data);
    Qt::DropAction action = drag->start();
    if (action == Qt::MoveAction && !control->isReadOnly() && drag->target() != q)
        control->removeSelection();
}

#endif // QT_NO_DRAGANDDROP

QT_END_NAMESPACE

#endif