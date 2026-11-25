/**
 * @file ImageDialogs.cpp
 * @brief 이미지 대화 상자 구현
 */
#include "pch.h"
#include "UI/ImageDialogs.h"

#include <cmath>

#include "Core/ImageCommands.h"
#include "Resource.h"

namespace
{
constexpr int kMaxSkew = 89;

bool IsValidSide(long value)
{
    return value >= 1 && value <= kMaxCanvasSide;
}
} // namespace

// ---------------------------------------------------------------------------
// CResizeDlg
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CResizeDlg, CDialog)
    ON_BN_CLICKED(IDC_RESIZE_PERCENT, &CResizeDlg::OnModeChanged)
    ON_BN_CLICKED(IDC_RESIZE_PIXELS, &CResizeDlg::OnModeChanged)
    ON_EN_CHANGE(IDC_RESIZE_WIDTH, &CResizeDlg::OnWidthChanged)
    ON_EN_CHANGE(IDC_RESIZE_HEIGHT, &CResizeDlg::OnHeightChanged)
END_MESSAGE_MAP()

CResizeDlg::CResizeDlg(CSize currentSize, CWnd* parent)
    : CDialog(IDD_RESIZE, parent), m_current(currentSize), m_newSize(currentSize)
{
}

BOOL CResizeDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_updating = true;
    CheckRadioButton(IDC_RESIZE_PERCENT, IDC_RESIZE_PIXELS, IDC_RESIZE_PERCENT);
    CheckDlgButton(IDC_RESIZE_KEEP, BST_CHECKED);
    WriteInt(IDC_RESIZE_WIDTH, 100);
    WriteInt(IDC_RESIZE_HEIGHT, 100);
    WriteInt(IDC_SKEW_H, 0);
    WriteInt(IDC_SKEW_V, 0);
    m_percentMode = true;
    m_updating = false;
    return TRUE;
}

bool CResizeDlg::IsPercent() const
{
    return IsDlgButtonChecked(IDC_RESIZE_PERCENT) == BST_CHECKED;
}

bool CResizeDlg::KeepRatio() const
{
    return IsDlgButtonChecked(IDC_RESIZE_KEEP) == BST_CHECKED;
}

int CResizeDlg::ReadInt(UINT id, bool& ok, bool allowNegative) const
{
    BOOL translated = FALSE;
    const int value = static_cast<int>(GetDlgItemInt(id, &translated, allowNegative ? TRUE : FALSE));
    ok = translated != FALSE;
    return value;
}

void CResizeDlg::WriteInt(UINT id, int value)
{
    SetDlgItemInt(id, static_cast<UINT>(value), TRUE);
}

void CResizeDlg::OnModeChanged()
{
    const bool percent = IsPercent();
    if (percent == m_percentMode)
    {
        return;
    }
    // 입력된 값을 새 기준으로 바꿔 줍니다.
    bool okW = false;
    bool okH = false;
    const int w = ReadInt(IDC_RESIZE_WIDTH, okW);
    const int h = ReadInt(IDC_RESIZE_HEIGHT, okH);
    m_updating = true;
    if (percent)
    {
        WriteInt(IDC_RESIZE_WIDTH, okW ? static_cast<int>(std::lround(w * 100.0 / m_current.cx)) : 100);
        WriteInt(IDC_RESIZE_HEIGHT, okH ? static_cast<int>(std::lround(h * 100.0 / m_current.cy)) : 100);
    }
    else
    {
        WriteInt(IDC_RESIZE_WIDTH, okW ? static_cast<int>(std::lround(m_current.cx * w / 100.0)) : m_current.cx);
        WriteInt(IDC_RESIZE_HEIGHT, okH ? static_cast<int>(std::lround(m_current.cy * h / 100.0)) : m_current.cy);
    }
    m_percentMode = percent;
    m_updating = false;
}

void CResizeDlg::OnWidthChanged()
{
    if (m_updating || !KeepRatio())
    {
        return;
    }
    bool ok = false;
    const int w = ReadInt(IDC_RESIZE_WIDTH, ok);
    if (!ok)
    {
        return;
    }
    m_updating = true;
    WriteInt(IDC_RESIZE_HEIGHT, IsPercent() ? w : static_cast<int>(std::lround(w * double(m_current.cy) / m_current.cx)));
    m_updating = false;
}

void CResizeDlg::OnHeightChanged()
{
    if (m_updating || !KeepRatio())
    {
        return;
    }
    bool ok = false;
    const int h = ReadInt(IDC_RESIZE_HEIGHT, ok);
    if (!ok)
    {
        return;
    }
    m_updating = true;
    WriteInt(IDC_RESIZE_WIDTH, IsPercent() ? h : static_cast<int>(std::lround(h * double(m_current.cx) / m_current.cy)));
    m_updating = false;
}

void CResizeDlg::OnOK()
{
    bool okW = false;
    bool okH = false;
    bool okSH = false;
    bool okSV = false;
    const int w = ReadInt(IDC_RESIZE_WIDTH, okW);
    const int h = ReadInt(IDC_RESIZE_HEIGHT, okH);
    const int skewH = ReadInt(IDC_SKEW_H, okSH, true);
    const int skewV = ReadInt(IDC_SKEW_V, okSV, true);

    CSize size = IsPercent() ? CSize(static_cast<int>(std::lround(m_current.cx * w / 100.0)),
                                     static_cast<int>(std::lround(m_current.cy * h / 100.0)))
                             : CSize(w, h);
    if (!okW || !okH || !IsValidSide(size.cx) || !IsValidSide(size.cy))
    {
        CString message;
        message.Format(L"크기는 1 ~ %d 픽셀 사이가 되도록 입력해 주세요.", kMaxCanvasSide);
        AfxMessageBox(message, MB_ICONWARNING);
        return;
    }
    if (!okSH || !okSV || skewH < -kMaxSkew || skewH > kMaxSkew || skewV < -kMaxSkew || skewV > kMaxSkew)
    {
        AfxMessageBox(L"기울이기 각도는 -89 ~ 89 사이로 입력해 주세요.", MB_ICONWARNING);
        return;
    }
    const ResizeSkewCommand check(size, skewH, skewV, 0);
    if (!IsValidSide(check.ResultSize().cx) || !IsValidSide(check.ResultSize().cy))
    {
        AfxMessageBox(L"기울인 결과가 너무 큽니다. 각도를 줄여 주세요.", MB_ICONWARNING);
        return;
    }

    m_newSize = size;
    m_skewH = skewH;
    m_skewV = skewV;
    CDialog::OnOK();
}

// ---------------------------------------------------------------------------
// CPropertiesDlg
// ---------------------------------------------------------------------------

CPropertiesDlg::CPropertiesDlg(CSize currentSize, CWnd* parent)
    : CDialog(IDD_PROPERTIES, parent), m_newSize(currentSize)
{
}

BOOL CPropertiesDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetDlgItemInt(IDC_PROP_WIDTH, static_cast<UINT>(m_newSize.cx), FALSE);
    SetDlgItemInt(IDC_PROP_HEIGHT, static_cast<UINT>(m_newSize.cy), FALSE);
    return TRUE;
}

void CPropertiesDlg::OnOK()
{
    BOOL okW = FALSE;
    BOOL okH = FALSE;
    const long w = static_cast<long>(GetDlgItemInt(IDC_PROP_WIDTH, &okW, FALSE));
    const long h = static_cast<long>(GetDlgItemInt(IDC_PROP_HEIGHT, &okH, FALSE));
    if (!okW || !okH || !IsValidSide(w) || !IsValidSide(h))
    {
        CString message;
        message.Format(L"크기는 1 ~ %d 픽셀 사이로 입력해 주세요.", kMaxCanvasSide);
        AfxMessageBox(message, MB_ICONWARNING);
        return;
    }
    m_newSize = CSize(w, h);
    CDialog::OnOK();
}
