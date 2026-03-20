#pragma once

#include <windows.h>

#include <algorithm>
#include <gdiplus.h>
#include <string>
#include <utility>
#include <vector>

#include "ui/window.hpp"

namespace tsf {

class CandidateWindow final : public Window {
public:
    void set_layout_columns(std::size_t columns) {
        layout_columns_ = std::clamp<std::size_t>(columns, 1, max_layout_columns);
        if (number_column_ >= layout_columns_) {
            number_column_ = 0;
        }
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::set_layout_columns " + std::to_wstring(layout_columns_));
    }

    void set_number_column(std::size_t column) {
        number_column_ = std::min(column, layout_columns_ - 1);
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::set_number_column " + std::to_wstring(number_column_));
    }

    void update_candidates(const std::vector<std::wstring>& values) {
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::update_candidates input_count=" + std::to_wstring(values.size()));
        candidates_ = values;
        if (candidates_.size() > max_visible_candidates) {
            candidates_.resize(max_visible_candidates);
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::update_candidates truncated_count=" + std::to_wstring(candidates_.size()));
        }
        for (std::size_t i = 0; i < candidates_.size(); ++i) {
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::candidate[" + std::to_wstring(i) + L"]=" + candidates_[i]);
        }

        if (candidates_.empty()) {
            selection_index_ = 0;
            DebugSink::instance().send(L"UI", L"CandidateWindow::update_candidates empty -> hide");
            hide();
            return;
        }

        if (selection_index_ >= candidates_.size()) {
            selection_index_ = candidates_.size() - 1;
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::update_candidates clamp selection=" + std::to_wstring(selection_index_));
        }

        if (!ensure_window()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::update_candidates ensure_window failed");
            return;
        }
        resize_to_layout();
        invalidate(FALSE);
    }

    void set_selection(std::size_t index) {
        if (candidates_.empty()) {
            selection_index_ = 0;
            DebugSink::instance().send(L"UI", L"CandidateWindow::set_selection ignored: empty candidates");
            return;
        }
        selection_index_ = std::min(index, candidates_.size() - 1);
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::set_selection index=" + std::to_wstring(index) +
                      L", effective=" + std::to_wstring(selection_index_));
        invalidate(FALSE);
    }

    void show_near_cursor() {
        if (candidates_.empty()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::show_near_cursor empty -> hide");
            hide();
            return;
        }
        if (!ensure_window()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::show_near_cursor ensure_window failed");
            return;
        }

        POINT cursor = {};
        if (GetCursorPos(&cursor) == 0) {
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::show_near_cursor GetCursorPos failed err=" + std::to_wstring(GetLastError()));
            return;
        }

        const auto [width, height] = client_size();
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::show_near_cursor cursor=(" + std::to_wstring(cursor.x) + L"," +
                      std::to_wstring(cursor.y) + L"), size=(" + std::to_wstring(width) + L"," +
                      std::to_wstring(height) + L")");
        show_at(cursor.x, cursor.y);
    }

    void show_at(int anchorX, int anchorY) {
        if (candidates_.empty()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::show_at empty -> hide");
            hide();
            return;
        }
        if (!ensure_window()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::show_at ensure_window failed");
            return;
        }

        const auto [width, height] = client_size();
        int x = anchorX + popup_offset_x;
        int y = anchorY + popup_offset_y;

        const POINT anchor = {anchorX, anchorY};
        const HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (GetMonitorInfoW(monitor, &monitorInfo) != 0) {
            if (x + width > monitorInfo.rcWork.right) {
                x = monitorInfo.rcWork.right - width;
            }
            if (x < monitorInfo.rcWork.left) {
                x = monitorInfo.rcWork.left;
            }
            if (y + height > monitorInfo.rcWork.bottom) {
                y = anchorY - height - 4;
            }
            if (y < monitorInfo.rcWork.top) {
                y = monitorInfo.rcWork.top;
            }
        }
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::show_at anchor=(" + std::to_wstring(anchorX) + L"," +
                      std::to_wstring(anchorY) + L"), final=(" + std::to_wstring(x) + L"," +
                      std::to_wstring(y) + L"), size=(" + std::to_wstring(width) + L"," + std::to_wstring(height) +
                      L")");
        set_window_pos(HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        show(SW_SHOWNOACTIVATE);
    }

private:
    static constexpr int popup_width = 128;
    static constexpr int expanded_popup_width = 410;
    static constexpr int popup_offset_x = 0;
    static constexpr int popup_offset_y = 4;
    static constexpr int top_padding = 4;
    static constexpr int row_height = 26;
    static constexpr int bottom_bar_height = 20;
    static constexpr int corner_radius = 10;
    static constexpr std::size_t max_visible_candidates = 36;
    static constexpr std::size_t max_layout_columns = 4;
    static constexpr int page_size = 9;

protected:
    const wchar_t* class_name() const noexcept override {
        return L"TSF_CandidatePopupWindow";
    }

    DWORD class_style() const noexcept override {
        return CS_HREDRAW | CS_VREDRAW;
    }

    LRESULT handle_message(UINT message, WPARAM wParam, LPARAM lParam) override {
        switch (message) {
            case WM_SIZE:
                DebugSink::instance().send(L"UI", L"CandidateWindow::handle_message WM_SIZE");
                apply_round_region();
                return 0;
            case WM_ERASEBKGND:
                DebugSink::instance().send(L"UI", L"CandidateWindow::handle_message WM_ERASEBKGND");
                return 1;
            case WM_PAINT:
                DebugSink::instance().send(L"UI", L"CandidateWindow::handle_message WM_PAINT");
                paint();
                return 0;
            default:
                break;
        }
        return Window::handle_message(message, wParam, lParam);
    }

private:
    class GdiplusSession {
    public:
        static bool ensure() {
            static GdiplusSession session;
            return session.ok_;
        }

    private:
        GdiplusSession() {
            Gdiplus::GdiplusStartupInput input;
            ok_ = (Gdiplus::GdiplusStartup(&token_, &input, nullptr) == Gdiplus::Ok);
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::GdiplusSession startup ok=" + std::wstring(ok_ ? L"TRUE" : L"FALSE") +
                          L", token=" + std::to_wstring(token_));
        }

        ~GdiplusSession() {
            if (token_ != 0) {
                Gdiplus::GdiplusShutdown(token_);
            }
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::GdiplusSession shutdown token=" + std::to_wstring(token_));
        }

    private:
        ULONG_PTR token_ = 0;
        bool ok_ = false;
    };

    static void build_rounded_path(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, Gdiplus::REAL radius) {
        const Gdiplus::REAL diameter = radius * 2.0f;
        path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
        path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
        path.AddArc(
            rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0.0f, 90.0f);
        path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
        path.CloseFigure();
    }

    bool ensure_window() {
        if (created()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::ensure_window already created");
            return true;
        }
        if (!GdiplusSession::ensure()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::ensure_window GDI+ not available");
            return false;
        }

        const auto [width, height] = client_size();
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::ensure_window creating size=(" + std::to_wstring(width) + L"," +
                      std::to_wstring(height) + L")");
        if (!create(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, WS_POPUP, L"IME Candidate List", CW_USEDEFAULT,
                CW_USEDEFAULT, width, height)) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::ensure_window create failed");
            return false;
        }
        apply_round_region();
        DebugSink::instance().send(L"UI", L"CandidateWindow::ensure_window created");
        return true;
    }

    std::pair<int, int> client_size() const {
        const int rows = std::max(1, std::min(page_size, static_cast<int>(candidates_.size())));
        const int height = top_padding + rows * row_height + bottom_bar_height;
        const int width = (layout_columns_ <= 1) ? popup_width : expanded_popup_width;
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::client_size rows=" + std::to_wstring(rows) +
                      L", width=" + std::to_wstring(width) + L", height=" + std::to_wstring(height));
        return {width, height};
    }

    void resize_to_layout() {
        if (!created()) {
            return;
        }
        const auto [width, height] = client_size();
        set_window_pos(nullptr, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
    }

    void apply_round_region() {
        if (!created()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::apply_round_region ignored: not created");
            return;
        }
        RECT rc = {};
        GetClientRect(hwnd(), &rc);
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        if (width <= 0 || height <= 0) {
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::apply_round_region invalid size width=" + std::to_wstring(width) +
                          L", height=" + std::to_wstring(height));
            return;
        }

        const int ellipse = corner_radius * 2;
        HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, ellipse, ellipse);
        if (!region) {
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::apply_round_region CreateRoundRectRgn failed err=" +
                          std::to_wstring(GetLastError()));
            return;
        }
        if (SetWindowRgn(hwnd(), region, TRUE) == 0) {
            DeleteObject(region);
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::apply_round_region SetWindowRgn failed err=" + std::to_wstring(GetLastError()));
            return;
        }
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::apply_round_region success width=" + std::to_wstring(width) +
                      L", height=" + std::to_wstring(height));
    }

    void paint() {
        DebugSink::instance().send(L"UI", L"CandidateWindow::paint begin");
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hwnd(), &ps);
        if (!hdc) {
            DebugSink::instance().send(
                L"UI", L"CandidateWindow::paint BeginPaint failed err=" + std::to_wstring(GetLastError()));
            return;
        }

        RECT client = {};
        GetClientRect(hwnd(), &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
        const HGDIOBJ oldBitmap = SelectObject(memDc, bitmap);
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::paint target size=(" + std::to_wstring(width) + L"," + std::to_wstring(height) +
                      L"), memDc=" + std::to_wstring(reinterpret_cast<ULONG_PTR>(memDc)) +
                      L", bitmap=" + std::to_wstring(reinterpret_cast<ULONG_PTR>(bitmap)));

        {
            Gdiplus::Graphics g(memDc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            draw_background(g, width, height);
            draw_candidates(g, width);
            draw_bottom_bar(g, width, height);
        }

        BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);

        SelectObject(memDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memDc);
        EndPaint(hwnd(), &ps);
        DebugSink::instance().send(L"UI", L"CandidateWindow::paint end");
    }

    void draw_background(Gdiplus::Graphics& g, int width, int height) {
        const Gdiplus::RectF bounds(
            0.5f, 0.5f, static_cast<Gdiplus::REAL>(width - 1), static_cast<Gdiplus::REAL>(height - 1));

        Gdiplus::GraphicsPath panelPath(Gdiplus::FillModeWinding);
        build_rounded_path(panelPath, bounds, static_cast<Gdiplus::REAL>(corner_radius));

        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(247, 247, 247));
        Gdiplus::Pen borderPen(Gdiplus::Color(216, 216, 216), 1.0f);
        g.FillPath(&bgBrush, &panelPath);
        g.DrawPath(&borderPen, &panelPath);
    }

    void draw_candidates(Gdiplus::Graphics& g, int width) {
        if (candidates_.empty()) {
            DebugSink::instance().send(L"UI", L"CandidateWindow::draw_candidates skipped: empty");
            return;
        }
        DebugSink::instance().send(
            L"UI", L"CandidateWindow::draw_candidates count=" + std::to_wstring(candidates_.size()) +
                      L", selection=" + std::to_wstring(selection_index_));

        Gdiplus::Font indexFont(L"Segoe UI", 12.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font valueFont(L"Microsoft JhengHei UI", 20.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        Gdiplus::SolidBrush indexBrush(Gdiplus::Color(102, 102, 102));
        Gdiplus::SolidBrush valueBrush(Gdiplus::Color(33, 33, 33));
        Gdiplus::SolidBrush selectionBrush(Gdiplus::Color(234, 234, 234));

        Gdiplus::StringFormat centerFormat;
        centerFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        centerFormat.SetAlignment(Gdiplus::StringAlignmentNear);

        const std::size_t columns = std::max<std::size_t>(1, std::min(layout_columns_, max_layout_columns));
        const Gdiplus::REAL columnWidth =
            (columns == 1) ? static_cast<Gdiplus::REAL>(width - 10)
                           : static_cast<Gdiplus::REAL>((width - 10 - static_cast<int>((columns - 1) * 8)) / columns);

        const int selection = static_cast<int>(selection_index_);
        const int selectionRow = selection % page_size;
        const int selectionCol = selection / page_size;
        const Gdiplus::REAL selectionX = 5.0f + selectionCol * (columnWidth + 8.0f);
        const int selectionTop = top_padding + selectionRow * row_height + 1;
        const Gdiplus::RectF selectedRect(
            selectionX, static_cast<Gdiplus::REAL>(selectionTop), columnWidth, static_cast<Gdiplus::REAL>(row_height - 2));
        Gdiplus::GraphicsPath selectedPath(Gdiplus::FillModeWinding);
        build_rounded_path(selectedPath, selectedRect, 5.0f);
        g.FillPath(&selectionBrush, &selectedPath);

        for (std::size_t i = 0; i < candidates_.size(); ++i) {
            const int row = static_cast<int>(i % page_size);
            const int col = static_cast<int>(i / page_size);
            const Gdiplus::REAL left = 5.0f + col * (columnWidth + 8.0f);
            const Gdiplus::REAL top = static_cast<Gdiplus::REAL>(top_padding + row * row_height);

            if (col == static_cast<int>(number_column_)) {
                const std::wstring index = std::to_wstring(row + 1);
                const Gdiplus::RectF indexRect(left + 7.0f, top, 13.0f, static_cast<Gdiplus::REAL>(row_height));
                g.DrawString(index.c_str(), -1, &indexFont, indexRect, &centerFormat, &indexBrush);
            }

            const Gdiplus::REAL textLeft = (col == static_cast<int>(number_column_)) ? (left + 25.0f) : (left + 8.0f);
            const Gdiplus::RectF valueRect(
                textLeft, top - 1.0f, columnWidth - (textLeft - left) - 6.0f, static_cast<Gdiplus::REAL>(row_height + 2));
            g.DrawString(candidates_[i].c_str(), -1, &valueFont, valueRect, &centerFormat, &valueBrush);
        }
    }

    void draw_bottom_bar(Gdiplus::Graphics& g, int width, int height) {
        const int lineY = height - bottom_bar_height;
        Gdiplus::Pen divider(Gdiplus::Color(218, 218, 218), 1.0f);
        g.DrawLine(
            &divider, 8.0f, static_cast<Gdiplus::REAL>(lineY), static_cast<Gdiplus::REAL>(width - 8),
            static_cast<Gdiplus::REAL>(lineY));

        Gdiplus::SolidBrush iconBrush(Gdiplus::Color(67, 67, 67));
        const Gdiplus::PointF leftTriangle[3] = {{10.0f, static_cast<Gdiplus::REAL>(lineY + 10)},
                                                 {16.0f, static_cast<Gdiplus::REAL>(lineY + 6)},
                                                 {16.0f, static_cast<Gdiplus::REAL>(lineY + 14)}};
        const Gdiplus::PointF rightTriangle[3] = {{30.0f, static_cast<Gdiplus::REAL>(lineY + 10)},
                                                  {24.0f, static_cast<Gdiplus::REAL>(lineY + 6)},
                                                  {24.0f, static_cast<Gdiplus::REAL>(lineY + 14)}};
        g.FillPolygon(&iconBrush, leftTriangle, 3);
        g.FillPolygon(&iconBrush, rightTriangle, 3);

        Gdiplus::Pen outlinePen(Gdiplus::Color(74, 74, 74), 1.0f);
        const Gdiplus::RectF enterRect(static_cast<Gdiplus::REAL>(width - 44), static_cast<Gdiplus::REAL>(lineY + 4),
                                       14.0f, 11.0f);
        g.DrawRectangle(&outlinePen, enterRect);
        g.DrawLine(&outlinePen, static_cast<Gdiplus::REAL>(width - 40), static_cast<Gdiplus::REAL>(lineY + 10),
                   static_cast<Gdiplus::REAL>(width - 34), static_cast<Gdiplus::REAL>(lineY + 10));
        g.DrawLine(&outlinePen, static_cast<Gdiplus::REAL>(width - 36), static_cast<Gdiplus::REAL>(lineY + 8),
                   static_cast<Gdiplus::REAL>(width - 34), static_cast<Gdiplus::REAL>(lineY + 10));
        g.DrawLine(&outlinePen, static_cast<Gdiplus::REAL>(width - 36), static_cast<Gdiplus::REAL>(lineY + 12),
                   static_cast<Gdiplus::REAL>(width - 34), static_cast<Gdiplus::REAL>(lineY + 10));

        g.FillEllipse(
            &iconBrush, static_cast<Gdiplus::REAL>(width - 18), static_cast<Gdiplus::REAL>(lineY + 7), 6.0f,
            6.0f);
    }

private:
    std::vector<std::wstring> candidates_;
    std::size_t selection_index_ = 0;
    std::size_t layout_columns_ = 1;
    std::size_t number_column_ = 0;
};

}  // namespace tsf
