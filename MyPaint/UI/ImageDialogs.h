#pragma once

/**
 * @file ImageDialogs.h
 * @brief [크기 조정 및 기울이기], [이미지 속성] 대화 상자
 */

/**
 * @brief 크기 조정 및 기울이기 대화 상자
 *
 * 기준을 백분율/픽셀 중에서 고르고, "가로 세로 비율 유지"를 켜면 한쪽을 바꿀 때 다른 쪽이 따라 바뀝니다.
 */
class CResizeDlg : public CDialog
{
public:
    CResizeDlg(CSize currentSize, CWnd* parent);

    CSize NewSize() const { return m_newSize; }
    int SkewHorizontal() const { return m_skewH; }
    int SkewVertical() const { return m_skewV; }

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg void OnModeChanged();
    afx_msg void OnWidthChanged();
    afx_msg void OnHeightChanged();
    DECLARE_MESSAGE_MAP()

private:
    bool IsPercent() const;
    bool KeepRatio() const;
    int ReadInt(UINT id, bool& ok, bool allowNegative = false) const;
    void WriteInt(UINT id, int value);

    CSize m_current;
    CSize m_newSize;
    int m_skewH = 0;
    int m_skewV = 0;
    bool m_percentMode = true; ///< 지금 입력 칸에 들어 있는 값의 기준
    bool m_updating = false;   ///< 비율 유지로 다른 칸을 고치는 중 (재귀 방지)
};

/**
 * @brief 이미지 속성 (캔버스 크기) 대화 상자
 */
class CPropertiesDlg : public CDialog
{
public:
    CPropertiesDlg(CSize currentSize, CWnd* parent);

    CSize NewSize() const { return m_newSize; }

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

private:
    CSize m_newSize;
};
