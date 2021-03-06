
#include "qwinquickapplicationwindow_p.h"
#include "qwinquickwindow.h"

#include <QEvent>
#include <QFocusEvent>
#include <qt_windows.h>
#include <QWindow>
#include <QQuickItem>

#include "Dwmapi.h"
#include <qdebug.h>


QWinQuickApplicationWindow::QWinQuickApplicationWindow(QWindow *parent)
    : QQuickApplicationWindow(parent)
{
    this->setFlag(Qt::FramelessWindowHint/* | Qt::WindowMinimizeButtonHint*/);
    //this->setAttribute(Qt::WA_TranslucentBackground, true);

    hWnd = (HWND)this->winId();
    DWORD style = ::GetWindowLong(hWnd, GWL_STYLE);
    ::SetWindowLong(hWnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION);
    style = ::GetWindowLong(hWnd, GWL_EXSTYLE);
    ::SetWindowLong(hWnd, GWL_EXSTYLE, style | WS_EX_APPWINDOW);


    //Clear margins & spacing & add the layout to prepare for the MainAppWidget
    //setContentsMargins(0, 0, 0, 0);
}

bool QWinQuickApplicationWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    BOOL bDwmEnabled = FALSE;
    LRESULT lRet = 0;
    HRESULT hr = S_OK;

    MSG *pmsg = (MSG *)message;
    bool procOk = false;
    WPARAM wParam = pmsg->wParam;
    LPARAM lParam = pmsg->lParam;
    UINT msg = pmsg->message;

    // Winproc worker for custom frame issues.
    hr = DwmIsCompositionEnabled(&bDwmEnabled);
    if (SUCCEEDED(hr))
    {
        lRet = CustomCaptionProc(msg, wParam, lParam, &procOk);
    }
    else
    {
        qDebug() << "HERE NO";
    }

    //very important
    *result = lRet;

    return procOk;
}

bool QWinQuickApplicationWindow::hitTopMenu(int x, int y)
{
    QPoint ori = position();
    int localX = x - ori.x();
    int localY = y - ori.y();

    QQuickItem *header = nullptr;
    auto root = contentItem()->childItems();
    if (root.empty() || !(header = dynamic_cast<QQuickItem *>(root[0])))
    {
        return localY < 20;
    }

    QVariant returnedValue = false;
    QVariant msg1 = localX;
    QVariant msg2 = localY;
    QMetaObject::invokeMethod(header, "hitMenuBar",
        Q_RETURN_ARG(QVariant, returnedValue),
        Q_ARG(QVariant, msg1),
        Q_ARG(QVariant, msg2));

    return returnedValue.toBool();
}

LRESULT QWinQuickApplicationWindow::CustomCaptionProc(UINT message, WPARAM wParam, LPARAM lParam, bool *procOk)
{
    LRESULT lRet = 0;
    HRESULT hr = S_OK;

    *procOk = DwmDefWindowProc(hWnd, message, wParam, lParam, &lRet);

    // Handle window activation.
    //if (message == WM_ACTIVATE)
    //{
    //    // Extend the frame into the client area.
    //    MARGINS margins;

    //    margins.cxLeftWidth = LEFTEXTENDWIDTH;
    //    margins.cxRightWidth = RIGHTEXTENDWIDTH;
    //    margins.cyBottomHeight = BOTTOMEXTENDWIDTH;
    //    margins.cyTopHeight = TOPEXTENDWIDTH;

    //    hr = DwmExtendFrameIntoClientArea(hWnd, &margins);

    //    if (!SUCCEEDED(hr))
    //    {
    //        // Handle error.
    //    }
    //    lRet = 0;
    //    //!
    //    *procOk = true;
    //}

    // Handle the non-client size message.
    if ((message == WM_NCCALCSIZE) && (wParam == TRUE))
    {
        lRet = 0;

        //!
        *procOk = true;
    }

    if (/*false && */message == WM_GETMINMAXINFO)
    {
        //if (::IsZoomed(hWnd)) {
        //    // 最大化时会超出屏幕，所以填充边框间距
        //    RECT frame = { 0, 0, 0, 0 };
        //    AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);
        //    frame.left = abs(frame.left);
        //    frame.top = abs(frame.bottom);
        //    q_ptr->setContentsMargins(frame.left, frame.top, frame.right, frame.bottom);
        //}
        //else {
        //    q_ptr->setContentsMargins(2, 2, 2, 2);
        //}

        MINMAXINFO * psinf = (MINMAXINFO *)lParam;
        psinf->ptMinTrackSize.x = minimumWidth();
        psinf->ptMinTrackSize.y = minimumHeight();

        lRet = ::DefWindowProc(hWnd, message, wParam, lParam);
        *procOk = true;
    }

    // Handle hit testing in the NCA if not handled by DwmDefWindowProc.
    if ((message == WM_NCHITTEST) && (lRet == 0))
    {
        lRet = HitTestNCA(wParam, lParam);

        if (lRet != HTNOWHERE)
        {
            *procOk = true;
        }
    }

    return lRet;
}

LRESULT QWinQuickApplicationWindow::HitTestNCA(WPARAM wParam, LPARAM lParam)
{
    // Get the point coordinates for the hit test.
    POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

    // Get the window rectangle.
    RECT rcWindow;
    GetWindowRect(hWnd, &rcWindow);

    // Get the frame rectangle, adjusted for the style without a caption.
    RECT rcFrame = { 0 };
    AdjustWindowRectEx(&rcFrame, WS_OVERLAPPEDWINDOW & ~WS_CAPTION, FALSE, NULL);

    // Determine if the hit test is for resizing. Default middle (1,1).
    USHORT uRow = 1;
    USHORT uCol = 1;
    bool fOnResizeBorder = false;

    // Determine if the point is at the top or bottom of the window.
    if (hitTopMenu(ptMouse.x, ptMouse.y))
    //if (ptMouse.y >= rcWindow.top && ptMouse.y < rcWindow.top + TOPEXTENDWIDTH)
    {
        fOnResizeBorder = (ptMouse.y < (rcWindow.top - rcFrame.top));
        uRow = 0;
    }
    else if (ptMouse.y < rcWindow.bottom && ptMouse.y >= rcWindow.bottom - BOTTOMEXTENDWIDTH)
    {
        uRow = 2;
    }

    // Determine if the point is at the left or right of the window.
    if (ptMouse.x >= rcWindow.left && ptMouse.x < rcWindow.left + LEFTEXTENDWIDTH)
    {
        uCol = 0; // left side
    }
    else if (ptMouse.x < rcWindow.right && ptMouse.x >= rcWindow.right - RIGHTEXTENDWIDTH)
    {
        uCol = 2; // right side
    }

    // Hit test (HTTOPLEFT, ... HTBOTTOMRIGHT)
    LRESULT hitTests[3][3] =
    {
        { HTTOPLEFT,    fOnResizeBorder ? HTTOP : HTCAPTION,    HTTOPRIGHT },
        { HTLEFT,       HTNOWHERE,     HTRIGHT },
        { HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT },
    };

    return hitTests[uRow][uCol];
}

void QWinQuickWindow::qmlRegisterType(const char *uri, int versionMajor, int versionMinor, const char *qmlName)
{
    ::qmlRegisterType<QWinQuickApplicationWindow>(uri, versionMajor, versionMinor, qmlName);
}
