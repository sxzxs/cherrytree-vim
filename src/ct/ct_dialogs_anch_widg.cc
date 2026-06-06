/*
 * ct_dialogs_anch_widg.cc
 *
 * Copyright 2009-2026
 * Giuseppe Penone <giuspen@gmail.com>
 * Evgenii Gurianov <https://github.com/txe>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "ct_dialogs.h"
#include "ct_main_win.h"
#include "ct_text_view.h"
#include <algorithm>
#include <cmath>
#if GTKMM_MAJOR_VERSION >= 4
#include <gdkmm/general.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/gesturedrag.h>
#endif

namespace {

#if GTKMM_MAJOR_VERSION >= 4
int _run_dialog_blocking(Gtk::Dialog& dialog)
{
    int response = Gtk::ResponseType::NONE;
    auto loop = Glib::MainLoop::create(false);
    dialog.signal_response().connect([&](int resp) {
        response = resp;
        dialog.hide();
        if (loop->is_running()) {
            loop->quit();
        }
    });
    dialog.signal_hide().connect([&]() {
        if (loop->is_running()) {
            loop->quit();
        }
    });
    dialog.present();
    loop->run();
    return response;
}

class CropImageGtk4
{
public:
    explicit CropImageGtk4(Glib::RefPtr<Gdk::Pixbuf> pixbuf)
    {
        _area.set_halign(Gtk::Align::START);
        _area.set_valign(Gtk::Align::START);
        _area.set_hexpand(false);
        _area.set_vexpand(false);
        _area.set_draw_func(sigc::mem_fun(*this, &CropImageGtk4::on_draw));

        _drag = Gtk::GestureDrag::create();
        _drag->set_button(GDK_BUTTON_PRIMARY);
        _drag->signal_drag_begin().connect(sigc::mem_fun(*this, &CropImageGtk4::on_drag_begin));
        _drag->signal_drag_update().connect(sigc::mem_fun(*this, &CropImageGtk4::on_drag_update));
        _drag->signal_drag_end().connect(sigc::mem_fun(*this, &CropImageGtk4::on_drag_end));
        _area.add_controller(_drag);

        _reset_click = Gtk::GestureClick::create();
        _reset_click->set_button(GDK_BUTTON_SECONDARY);
        _reset_click->signal_pressed().connect([this](int, double, double) {
            reset_to_full_image();
        });
        _area.add_controller(_reset_click);

        set(pixbuf);
    }

    Gtk::DrawingArea& widget()
    {
        return _area;
    }

    void set(Glib::RefPtr<Gdk::Pixbuf> pixbuf)
    {
        if (!pixbuf) {
            return;
        }
        if (_pixbuf) {
            const double x_scale = static_cast<double>(pixbuf->get_width()) / static_cast<double>(_pixbuf->get_width());
            const double y_scale = static_cast<double>(pixbuf->get_height()) / static_cast<double>(_pixbuf->get_height());
            _x *= x_scale;
            _w *= x_scale;
            _y *= y_scale;
            _h *= y_scale;
        }
        _pixbuf = pixbuf;
        _area.set_content_width(_pixbuf->get_width());
        _area.set_content_height(_pixbuf->get_height());
        _area.set_size_request(_pixbuf->get_width(), _pixbuf->get_height());
        normalize_crop();
        _area.queue_draw();
    }

    void get_crop(int width, int height, double* rx, double* ry, double* rw, double* rh)
    {
        normalize_crop();
        if (!_pixbuf) {
            if (rx) *rx = 0;
            if (ry) *ry = 0;
            if (rw) *rw = width;
            if (rh) *rh = height;
            return;
        }

        const double w_scale = static_cast<double>(width) / static_cast<double>(_pixbuf->get_width());
        const double h_scale = static_cast<double>(height) / static_cast<double>(_pixbuf->get_height());
        if (rx) *rx = _x * w_scale;
        if (ry) *ry = _y * h_scale;
        if (rw) *rw = _w * w_scale;
        if (rh) *rh = _h * h_scale;
    }

private:
    static double clamp_to(double value, double upper)
    {
        return std::max(0.0, std::min(value, upper));
    }

    void reset_to_full_image()
    {
        if (!_pixbuf) {
            return;
        }
        _x = 0;
        _y = 0;
        _w = _pixbuf->get_width();
        _h = _pixbuf->get_height();
        _moved = false;
        _area.queue_draw();
    }

    void normalize_crop()
    {
        if (!_pixbuf) {
            return;
        }
        if (_w < 0) {
            _x += _w;
            _w *= -1;
        }
        if (_h < 0) {
            _y += _h;
            _h *= -1;
        }

        const double max_w = _pixbuf->get_width();
        const double max_h = _pixbuf->get_height();
        _x = clamp_to(_x, max_w);
        _y = clamp_to(_y, max_h);
        _w = clamp_to(_w, max_w - _x);
        _h = clamp_to(_h, max_h - _y);
        if (_w < 1.0 or _h < 1.0) {
            reset_to_full_image();
        }
    }

    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int, int)
    {
        if (!_pixbuf) {
            return;
        }
        Gdk::Cairo::set_source_pixbuf(cr, _pixbuf, 0, 0);
        cr->paint();

        double x = _x;
        double y = _y;
        double w = _w;
        double h = _h;
        if (w < 0) {
            x += w;
            w *= -1;
        }
        if (h < 0) {
            y += h;
            h *= -1;
        }

        cr->save();
        cr->set_source_rgba(0, 0, 0, 0.25);
        cr->set_fill_rule(Cairo::Context::FillRule::EVEN_ODD);
        cr->rectangle(0, 0, _pixbuf->get_width(), _pixbuf->get_height());
        cr->rectangle(x, y, w, h);
        cr->fill();
        cr->restore();

        cr->save();
        cr->set_source_rgb(1, 1, 1);
        cr->set_line_width(1.0);
        cr->rectangle(x + 0.5, y + 0.5, std::max(0.0, w - 1.0), std::max(0.0, h - 1.0));
        cr->stroke();
        cr->restore();
    }

    void on_drag_begin(double start_x, double start_y)
    {
        if (!_pixbuf) {
            return;
        }
        _x = clamp_to(start_x, _pixbuf->get_width());
        _y = clamp_to(start_y, _pixbuf->get_height());
        _w = 0;
        _h = 0;
        _moved = false;
        _area.queue_draw();
    }

    void on_drag_update(double offset_x, double offset_y)
    {
        if (!_pixbuf) {
            return;
        }
        _w = clamp_to(_x + offset_x, _pixbuf->get_width()) - _x;
        _h = clamp_to(_y + offset_y, _pixbuf->get_height()) - _y;
        _moved = true;
        _area.queue_draw();
    }

    void on_drag_end(double offset_x, double offset_y)
    {
        if (!_pixbuf) {
            return;
        }
        _w = clamp_to(_x + offset_x, _pixbuf->get_width()) - _x;
        _h = clamp_to(_y + offset_y, _pixbuf->get_height()) - _y;
        if (!_moved or std::abs(_w) < 1.0 or std::abs(_h) < 1.0) {
            reset_to_full_image();
            return;
        }
        normalize_crop();
        _area.queue_draw();
    }

private:
    Gtk::DrawingArea _area;
    Glib::RefPtr<Gtk::GestureDrag> _drag;
    Glib::RefPtr<Gtk::GestureClick> _reset_click;
    Glib::RefPtr<Gdk::Pixbuf> _pixbuf;
    double _x{0};
    double _y{0};
    double _w{0};
    double _h{0};
    bool _moved{false};
};
#endif

}

#if GTKMM_MAJOR_VERSION < 4 && !defined(GTKMM_DISABLE_DEPRECATED)
Glib::ustring CtDialogs::latex_handle_dialog(CtMainWin* pCtMainWin,
                                             const Glib::ustring& latex_text)
{
    CtTextView textView{pCtMainWin};
    Glib::RefPtr<Gtk::TextBuffer> rBuffer = textView.get_buffer();
    rBuffer->set_text(latex_text);
    textView.setup_for_syntax("latex");
    pCtMainWin->apply_syntax_highlighting(rBuffer, "latex", false/*forceReApply*/);
    auto scrolledwindow = Gtk::manage(new Gtk::ScrolledWindow{});
    scrolledwindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledwindow->add(textView.mm());
    Gtk::Dialog dialog{_("Latex Text"),
                       *pCtMainWin,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT};

    (void)CtMiscUtil::dialog_add_button(&dialog, _("Cancel"), Gtk::RESPONSE_REJECT, "ct_cancel");
    (void)CtMiscUtil::dialog_add_button(&dialog, _("OK"), Gtk::RESPONSE_ACCEPT, "ct_done", true/*isDefault*/);

    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(400, 250);
    Gtk::Box* pContentArea = dialog.get_content_area();
    auto hbox = Gtk::manage(new Gtk::Box{Gtk::ORIENTATION_HORIZONTAL, 2/*spacing*/});
    auto vbox = Gtk::manage(new Gtk::Box{Gtk::ORIENTATION_VERTICAL, 2/*spacing*/});
    hbox->pack_start(*scrolledwindow);
    hbox->pack_start(*vbox, false, false);
    auto pCtConfig = pCtMainWin->get_ct_config();
    auto label_latex_size = Gtk::manage(new Gtk::Label{_("Image Size dpi")});
    Glib::RefPtr<Gtk::Adjustment> adj_latex_size = Gtk::Adjustment::create(pCtConfig->latexSizeDpi, 1, 10000, 10);
    auto spinbutton_latex_size = Gtk::manage(new Gtk::SpinButton{adj_latex_size});
    vbox->pack_end(*spinbutton_latex_size, false, false);
    vbox->pack_end(*label_latex_size, false, false);
    auto button_latex_tutorial = Gtk::manage(new Gtk::Button{});
    auto button_latex_reference = Gtk::manage(new Gtk::Button{});
    button_latex_tutorial->set_label(_("Tutorial"));
    button_latex_reference->set_label(_("Reference"));
    button_latex_tutorial->set_image_from_icon_name("ct_link_website", Gtk::ICON_SIZE_MENU);
    button_latex_reference->set_image_from_icon_name("ct_link_website", Gtk::ICON_SIZE_MENU);
    button_latex_tutorial->set_tooltip_text(_("LaTeX Math and Equations Tutorial"));
    button_latex_reference->set_tooltip_text(_("LaTeX Math Symbols Reference"));
    button_latex_tutorial->set_always_show_image(true);
    button_latex_reference->set_always_show_image(true);
    vbox->pack_start(*button_latex_tutorial, false, false);
    vbox->pack_start(*button_latex_reference, false, false);
    Glib::ustring error_msg = CtImageLatex::getRenderingErrorMessage(&latex_text);
    if (not error_msg.empty()) {
        auto p_label_error_msg = Gtk::manage(new Gtk::Label{error_msg});
        p_label_error_msg->set_use_markup(true);
        pContentArea->pack_start(*p_label_error_msg);
    }
    pContentArea->pack_start(*hbox);
    spinbutton_latex_size->signal_value_changed().connect([pCtMainWin, spinbutton_latex_size](){
        pCtMainWin->get_ct_config()->latexSizeDpi = spinbutton_latex_size->get_value_as_int();
    });
    button_latex_tutorial->signal_clicked().connect([](){
        fs::open_weblink("https://latex-tutorial.com/tutorials/amsmath/");
    });
    button_latex_reference->signal_clicked().connect([](){
        fs::open_weblink("https://latex-tutorial.com/symbols/math-symbols/");
    });
    auto on_key_press_dialog = [&](GdkEventKey* pEventKey)->bool{
        if (GDK_KEY_Escape == pEventKey->keyval) {
            Gtk::Button* pButton = static_cast<Gtk::Button*>(dialog.get_widget_for_response(Gtk::RESPONSE_REJECT));
            pButton->grab_focus();
            pButton->clicked();
            return true;
        }
        return false;
    };
    dialog.signal_key_press_event().connect(on_key_press_dialog, false/*call me before other*/);
    pContentArea->show_all();
    return Gtk::RESPONSE_ACCEPT == dialog.run() ? rBuffer->get_text() : "";
}

class CropImage : public Gtk::Image {

public:
    CropImage(Glib::RefPtr<Gdk::Pixbuf> pixbuf) :
            Gtk::Image(pixbuf) {

        x = 0;
        y = 0;
        w = pixbuf->get_width();
        h = pixbuf->get_height();
        moved = false;

        set_has_window(true);

        add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
            Gdk::POINTER_MOTION_MASK);

    }

    void set(Glib::RefPtr<Gdk::Pixbuf> pixbuf) {

        int old_width = get_pixbuf()->get_width();
        int old_height = get_pixbuf()->get_height();
        int new_width = pixbuf->get_width();
        int new_height = pixbuf->get_height();

        Gtk::Image::set(pixbuf);

        x *= (double) new_width / old_width;
        w *= (double) new_width / old_width;
        y *= (double) new_height / old_height;
        h *= (double) new_height / old_height;

    }

    /* Get result x/y/w/h given that actual image dimensions are
     * width × height */
    void get_crop(int width, int height,
                  double* rx, double* ry, double* rw, double* rh) {

        if (w < 0) {
            x += w;
            w *= -1;
        }
        if (h < 0) {
            y += h;
            h *= -1;
        }

        double wscale = (double) width / get_pixbuf()->get_width();
        double hscale = (double) height / get_pixbuf()->get_height();

        if (rx) *rx = x * wscale;
        if (rw) *rw = w * wscale;
        if (ry) *ry = y * hscale;
        if (rh) *rh = h * hscale;

    }

protected:

    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {

        cr->save();
        Gdk::Cairo::set_source_pixbuf( cr, get_pixbuf(), 0, 0 );
        cr->paint();
        cr->restore();

        int img_w = get_pixbuf()->get_width();
        int img_h = get_pixbuf()->get_height();

        cr->save();
        cr->set_source_rgba(0, 0, 0, 0.25);
        cr->set_fill_rule(Cairo::FILL_RULE_EVEN_ODD);
        cr->rectangle(0, 0, img_w, img_h);
        cr->rectangle(x, y, w, h);
        cr->fill();
        cr->restore();

        return false;

    }

    bool on_motion_notify_event(GdkEventMotion* event) override {

        if (event->state & GDK_BUTTON1_MASK) {

            moved = true;
            w = event->x - x;
            h = event->y - y;
            queue_draw();

        }

        return true;

    }

    bool on_button_press_event(GdkEventButton* event) override {

        if (GDK_BUTTON_PRIMARY == event->button) {
            x = event->x;
            y = event->y;
            w = 0;
            h = 0;
            moved = false;
        }
        else if (GDK_BUTTON_SECONDARY == event->button) {
            x = 0;
            y = 0;
            w = get_pixbuf()->get_width();
            h = get_pixbuf()->get_height();
        }

        queue_draw();

        return true;

    }

    bool on_button_release_event(GdkEventButton* event) override {

        if (GDK_BUTTON_PRIMARY == event->button && !moved) {
            x = 0;
            y = 0;
            w = get_pixbuf()->get_width();
            h = get_pixbuf()->get_height();
            queue_draw();
        }

        return true;

    }

private:
    int x, y, w, h;
    bool moved;

};

Glib::RefPtr<Gdk::Pixbuf> CtDialogs::image_handle_dialog(Gtk::Window& parent_win,
                                                         Glib::RefPtr<Gdk::Pixbuf> rOriginalPixbuf)
{
    int width = rOriginalPixbuf->get_width();
    int height = rOriginalPixbuf->get_height();
    double image_w_h_ration = static_cast<double>(width)/height;

    Gtk::Dialog dialog{_("Image Properties"),
                       parent_win,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT};

    (void)CtMiscUtil::dialog_add_button(&dialog, _("Cancel"), Gtk::RESPONSE_REJECT, "ct_cancel");
    (void)CtMiscUtil::dialog_add_button(&dialog, _("OK"), Gtk::RESPONSE_ACCEPT, "ct_done", true/*isDefault*/);

    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(600, 500);
    Gtk::Button button_rotate_90_ccw;
    button_rotate_90_ccw.set_image_from_icon_name("ct_rotate-left", Gtk::ICON_SIZE_DND);
    button_rotate_90_ccw.set_tooltip_text(_("Rotate Left"));
    Gtk::Button button_rotate_90_cw;
    button_rotate_90_cw.set_image_from_icon_name("ct_rotate-right", Gtk::ICON_SIZE_DND);
    button_rotate_90_cw.set_tooltip_text(_("Rotate Right"));
    Gtk::ScrolledWindow scrolledwindow;
    scrolledwindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    Glib::RefPtr<Gtk::Adjustment> rHAdj = Gtk::Adjustment::create(width, 1, height, 1);
    Glib::RefPtr<Gtk::Adjustment> rVAdj = Gtk::Adjustment::create(width, 1, width, 1);
    Gtk::Viewport viewport(rHAdj, rVAdj);
    CropImage image{rOriginalPixbuf};
    scrolledwindow.add(viewport);
    viewport.add(image);
    Gtk::Box hbox_1{Gtk::ORIENTATION_HORIZONTAL, 2/*spacing*/};
    hbox_1.pack_start(button_rotate_90_ccw, false, false);
    hbox_1.pack_start(scrolledwindow);
    hbox_1.pack_start(button_rotate_90_cw, false, false);
    Gtk::Button button_crop;
    button_crop.set_image_from_icon_name("ct_edit_cut", Gtk::ICON_SIZE_DND);
    button_crop.set_tooltip_text(_("In order to crop the image, select the area with the mouse before clicking OK"));
    Gtk::Button button_flip_horizontal;
    button_flip_horizontal.set_image_from_icon_name("ct_flip-horizontal", Gtk::ICON_SIZE_DND);
    button_flip_horizontal.set_tooltip_text(_("Flip Horizontally"));
    Gtk::Button button_flip_vertical;
    button_flip_vertical.set_image_from_icon_name("ct_flip-vertical", Gtk::ICON_SIZE_DND);
    button_flip_vertical.set_tooltip_text(_("Flip Vertically"));
    Gtk::Box hbox_2{Gtk::ORIENTATION_HORIZONTAL, 2/*spacing*/};
    hbox_2.pack_start(button_flip_horizontal, true, true);
    hbox_2.pack_start(button_crop, true, true);
    hbox_2.pack_start(button_flip_vertical, true, true);
    Gtk::Label label_width{_("Width")};
    Glib::RefPtr<Gtk::Adjustment> rAdj_width = Gtk::Adjustment::create(width, 1, 10000, 1);
    Gtk::SpinButton spinbutton_width{rAdj_width};
    Gtk::Label label_height{_("Height")};
    Glib::RefPtr<Gtk::Adjustment> rAdj_height = Gtk::Adjustment::create(height, 1, 10000, 1);
    Gtk::SpinButton spinbutton_height{rAdj_height};
    Gtk::Box hbox_3{Gtk::ORIENTATION_HORIZONTAL};
    hbox_3.pack_start(label_width);
    hbox_3.pack_start(spinbutton_width);
    hbox_3.pack_start(label_height);
    hbox_3.pack_start(spinbutton_height);
    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->pack_start(hbox_1);
    pContentArea->pack_start(hbox_2, false, false);
    pContentArea->pack_start(hbox_3, false, false);
    pContentArea->set_spacing(6);

    bool stop_update = false;
    auto image_load_into_dialog = [&]() {
        stop_update = true;
        spinbutton_width.set_value(width);
        spinbutton_height.set_value(height);
        Glib::RefPtr<Gdk::Pixbuf> rPixbuf;
        if (width <= 900 && height <= 600) {
            // original size into the dialog
            rPixbuf = rOriginalPixbuf->scale_simple(width, height, Gdk::INTERP_BILINEAR);
        }
        else {
            // reduced size visible into the dialog
            if (width > 900) {
                int img_parms_width = 900;
                int img_parms_height = (int)(img_parms_width / image_w_h_ration);
                rPixbuf = rOriginalPixbuf->scale_simple(img_parms_width, img_parms_height, Gdk::INTERP_BILINEAR);
            }
            else {
                int img_parms_height = 600;
                int img_parms_width = (int)(img_parms_height * image_w_h_ration);
                rPixbuf = rOriginalPixbuf->scale_simple(img_parms_width, img_parms_height, Gdk::INTERP_BILINEAR);
            }
        }
        image.set(rPixbuf);
        stop_update = false;
    };
    button_rotate_90_cw.signal_clicked().connect([&](){
        rOriginalPixbuf = rOriginalPixbuf->rotate_simple(Gdk::PixbufRotation::PIXBUF_ROTATE_CLOCKWISE);
        image_w_h_ration = 1./image_w_h_ration;
        std::swap(width, height); // new width is the former height and vice versa
        image_load_into_dialog();
    });
    button_rotate_90_ccw.signal_clicked().connect([&](){
        rOriginalPixbuf = rOriginalPixbuf->rotate_simple(Gdk::PixbufRotation::PIXBUF_ROTATE_COUNTERCLOCKWISE);
        image_w_h_ration = 1./image_w_h_ration;
        std::swap(width, height); // new width is the former height and vice versa
        image_load_into_dialog();
    });
    button_flip_horizontal.signal_clicked().connect([&](){
        rOriginalPixbuf = rOriginalPixbuf->flip(true);
        image_load_into_dialog();
    });
    button_crop.signal_clicked().connect([&](){
        CtDialogs::info_dialog(_("In order to crop the image, select the area with the mouse before clicking OK"), dialog);
    });
    button_flip_vertical.signal_clicked().connect([&](){
        rOriginalPixbuf = rOriginalPixbuf->flip(false);
        image_load_into_dialog();
    });
    spinbutton_width.signal_value_changed().connect([&](){
        if (stop_update) return;
        width = spinbutton_width.get_value_as_int();
        height = (int)(width/image_w_h_ration);
        image_load_into_dialog();
    });
    spinbutton_height.signal_value_changed().connect([&](){
        if (stop_update) return;
        height = spinbutton_height.get_value_as_int();
        width = (int)(height*image_w_h_ration);
        image_load_into_dialog();
    });
    auto on_key_press_dialog = [&](GdkEventKey* pEventKey)->bool{
        if (GDK_KEY_Return == pEventKey->keyval or GDK_KEY_KP_Enter == pEventKey->keyval) {
            Gtk::Button* pButton = static_cast<Gtk::Button*>(dialog.get_widget_for_response(Gtk::RESPONSE_ACCEPT));
            pButton->grab_focus();
            pButton->clicked();
            return true;
        }
        if (GDK_KEY_Escape == pEventKey->keyval) {
            Gtk::Button* pButton = static_cast<Gtk::Button*>(dialog.get_widget_for_response(Gtk::RESPONSE_REJECT));
            pButton->grab_focus();
            pButton->clicked();
            return true;
        }
        return false;
    };
    dialog.signal_key_press_event().connect(on_key_press_dialog, false/*call me before other*/);
    image_load_into_dialog();
    pContentArea->show_all();
    if ( Gtk::RESPONSE_ACCEPT == dialog.run() ) {
        double x, y, w, h;
        image.get_crop( width, height, &x, &y, &w, &h );
        Glib::RefPtr<Gdk::Pixbuf> rPixbuf = Gdk::Pixbuf::create(
            rOriginalPixbuf->get_colorspace(),
            rOriginalPixbuf->get_has_alpha(),
            rOriginalPixbuf->get_bits_per_sample(),
            w, h );
        rOriginalPixbuf->scale( rPixbuf,
            0, 0, /* Top left X & Y on dest pixbuf */
            w, h, /* Width & height of destination image */
            - x, - y, /* Top left on src, after scaling */
            (double) width / rOriginalPixbuf->get_width(), /* Scale */
            (double) height / rOriginalPixbuf->get_height(),
            Gdk::INTERP_BILINEAR );
        return rPixbuf;
    } else {
        return Glib::RefPtr<Gdk::Pixbuf>{};
    }
}

bool CtDialogs::codeboxhandle_dialog(CtMainWin* pCtMainWin,
                                     const Glib::ustring& title)
{
    Gtk::Dialog dialog{title,
                       *pCtMainWin,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT};

    (void)CtMiscUtil::dialog_add_button(&dialog, _("Cancel"), Gtk::RESPONSE_REJECT, "ct_cancel");
    (void)CtMiscUtil::dialog_add_button(&dialog, _("OK"), Gtk::RESPONSE_ACCEPT, "ct_done");

    dialog.set_default_size(300, -1);
    dialog.set_position(Gtk::WIN_POS_CENTER_ON_PARENT);

    CtConfig* pConfig = pCtMainWin->get_ct_config();

    Gtk::Button button_prog_lang;
    const Glib::ustring syntax_hl_id = pConfig->codeboxSynHighl != CtConst::PLAIN_TEXT_ID ? pConfig->codeboxSynHighl : pConfig->autoSynHighl;
    const std::string stock_id = pCtMainWin->get_code_icon_name(syntax_hl_id);
    button_prog_lang.set_label(syntax_hl_id);
    button_prog_lang.set_image(*pCtMainWin->new_managed_image_from_stock(stock_id, Gtk::ICON_SIZE_MENU));
    Gtk::RadioButton radiobutton_plain_text{_("Plain Text")};
    Gtk::RadioButton radiobutton_auto_syntax_highl{_("Automatic Syntax Highlighting")};
    radiobutton_auto_syntax_highl.join_group(radiobutton_plain_text);
    if (pConfig->codeboxSynHighl == CtConst::PLAIN_TEXT_ID) {
        radiobutton_plain_text.set_active(true);
        button_prog_lang.set_sensitive(false);
    }
    else {
        radiobutton_auto_syntax_highl.set_active(true);
    }
    Gtk::Box type_vbox{Gtk::ORIENTATION_VERTICAL};
    type_vbox.pack_start(radiobutton_plain_text);
    type_vbox.pack_start(radiobutton_auto_syntax_highl);
    type_vbox.pack_start(button_prog_lang);
    Gtk::Frame type_frame{Glib::ustring("<b>")+_("Type")+"</b>"};
    dynamic_cast<Gtk::Label*>(type_frame.get_label_widget())->set_use_markup(true);
    type_frame.set_shadow_type(Gtk::SHADOW_NONE);
    type_frame.add(type_vbox);

    Gtk::Label label_width{_("Width")};
    Glib::RefPtr<Gtk::Adjustment> rAdj_width = Gtk::Adjustment::create(pConfig->codeboxWidth, 1, 10000);
    Gtk::SpinButton spinbutton_width{rAdj_width};
    spinbutton_width.set_value(pConfig->codeboxWidth);
    Gtk::Label label_height{_("Height")};
    Glib::RefPtr<Gtk::Adjustment> rAdj_height = Gtk::Adjustment::create(pConfig->codeboxHeight, 1, 10000);
    Gtk::SpinButton spinbutton_height{rAdj_height};
    spinbutton_height.set_value(pConfig->codeboxHeight);

    Gtk::RadioButton radiobutton_codebox_pixels{_("pixels")};
    Gtk::RadioButton radiobutton_codebox_percent{"%"};
    radiobutton_codebox_percent.join_group(radiobutton_codebox_pixels);
    radiobutton_codebox_pixels.set_active(pConfig->codeboxWidthPixels);
    radiobutton_codebox_percent.set_active(!pConfig->codeboxWidthPixels);

    Gtk::Box vbox_pix_perc{Gtk::ORIENTATION_VERTICAL};
    vbox_pix_perc.pack_start(radiobutton_codebox_pixels);
    vbox_pix_perc.pack_start(radiobutton_codebox_percent);
    Gtk::Box hbox_width{Gtk::ORIENTATION_HORIZONTAL, 5/*spacing*/};
    hbox_width.pack_start(label_width, false, false);
    hbox_width.pack_start(spinbutton_width, false, false);
    hbox_width.pack_start(vbox_pix_perc);
    Gtk::Box hbox_height{Gtk::ORIENTATION_HORIZONTAL, 5/*spacing*/};
    hbox_height.pack_start(label_height, false, false);
    hbox_height.pack_start(spinbutton_height, false, false);
    Gtk::Box vbox_size{Gtk::ORIENTATION_VERTICAL};
    vbox_size.pack_start(hbox_width);
    vbox_size.pack_start(hbox_height);
    CtMiscUtil::set_widget_margins(vbox_size, 0, 6, 6, 6);

    Gtk::Frame size_frame{Glib::ustring("<b>")+_("Size")+"</b>"};
    dynamic_cast<Gtk::Label*>(size_frame.get_label_widget())->set_use_markup(true);
    size_frame.set_shadow_type(Gtk::SHADOW_NONE);
    size_frame.add(vbox_size);

    Gtk::CheckButton checkbutton_codebox_linenumbers{_("Show Line Numbers")};
    checkbutton_codebox_linenumbers.set_active(pConfig->codeboxLineNum);
    Gtk::CheckButton checkbutton_codebox_matchbrackets{_("Highlight Matching Brackets")};
    checkbutton_codebox_matchbrackets.set_active(pConfig->codeboxMatchBra);
    Gtk::Box vbox_options{Gtk::ORIENTATION_VERTICAL};
    vbox_options.pack_start(checkbutton_codebox_linenumbers);
    vbox_options.pack_start(checkbutton_codebox_matchbrackets);
    CtMiscUtil::set_widget_margins(vbox_options, 6, 6, 6, 6);

    Gtk::Frame options_frame{Glib::ustring("<b>")+_("Options")+"</b>"};
    dynamic_cast<Gtk::Label*>(options_frame.get_label_widget())->set_use_markup(true);
    options_frame.set_shadow_type(Gtk::SHADOW_NONE);
    options_frame.add(vbox_options);

    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->set_spacing(5);
    pContentArea->pack_start(type_frame);
    pContentArea->pack_start(size_frame);
    pContentArea->pack_start(options_frame);
    pContentArea->show_all();

    button_prog_lang.signal_clicked().connect([&button_prog_lang, &dialog, pCtMainWin, pConfig](){
        Glib::RefPtr<CtChooseDialogListStore> rItemStore = CtChooseDialogListStore::create();
        unsigned pathSelectIdx{0};
        unsigned pathCurrIdx{0};
        const auto currSyntaxHighl = button_prog_lang.get_label();
        const gchar * const * pLanguageIDs = gtk_source_language_manager_get_language_ids(pCtMainWin->get_language_manager());
        for (auto pLang = pLanguageIDs; *pLang; ++pLang) {
            rItemStore->add_row(pCtMainWin->get_code_icon_name(*pLang), "", *pLang);
            if (*pLang == currSyntaxHighl) {
                pathSelectIdx = pathCurrIdx;
            }
            ++pathCurrIdx;
        }
        Gtk::TreeModel::iterator res = CtDialogs::choose_item_dialog(dialog,
                                                          _("Automatic Syntax Highlighting"),
                                                          rItemStore,
                                                          nullptr/*single_column_name*/,
                                                          std::to_string(pathSelectIdx),
                                                          std::make_pair(200, pConfig->winRect[3]));
        if (res) {
            const Glib::ustring syntax_hl_id = res->get_value(rItemStore->columns.desc);
            const std::string stock_id = pCtMainWin->get_code_icon_name(syntax_hl_id);
            button_prog_lang.set_label(syntax_hl_id);
            button_prog_lang.set_image(*pCtMainWin->new_managed_image_from_stock(stock_id, Gtk::ICON_SIZE_MENU));
        }
    });
    radiobutton_auto_syntax_highl.signal_toggled().connect([&radiobutton_auto_syntax_highl, &button_prog_lang](){
        button_prog_lang.set_sensitive(radiobutton_auto_syntax_highl.get_active());
    });
    dialog.signal_key_press_event().connect([&](GdkEventKey* pEventKey){
        if (GDK_KEY_Return == pEventKey->keyval or GDK_KEY_KP_Enter == pEventKey->keyval) {
            spinbutton_width.update();
            spinbutton_height.update();
            dialog.response(Gtk::RESPONSE_ACCEPT);
            return true;
        }
        return false;
    });
    radiobutton_codebox_pixels.signal_toggled().connect([&radiobutton_codebox_pixels, &spinbutton_width](){
        if (radiobutton_codebox_pixels.get_active()) {
            spinbutton_width.set_value(700);
        }
        else if (spinbutton_width.get_value() > 100) {
            spinbutton_width.set_value(90);
        }
    });
    auto on_key_press_dialog = [&](GdkEventKey* pEventKey)->bool{
        if (GDK_KEY_Return == pEventKey->keyval or GDK_KEY_KP_Enter == pEventKey->keyval) {
            Gtk::Button* pButton = static_cast<Gtk::Button*>(dialog.get_widget_for_response(Gtk::RESPONSE_ACCEPT));
            pButton->grab_focus();
            pButton->clicked();
            return true;
        }
        if (GDK_KEY_Escape == pEventKey->keyval) {
            Gtk::Button* pButton = static_cast<Gtk::Button*>(dialog.get_widget_for_response(Gtk::RESPONSE_REJECT));
            pButton->grab_focus();
            pButton->clicked();
            return true;
        }
        return false;
    };
    dialog.signal_key_press_event().connect(on_key_press_dialog, false/*call me before other*/);

    const int response = dialog.run();
    dialog.hide();

    if (response == Gtk::RESPONSE_ACCEPT) {
        pConfig->codeboxWidth = spinbutton_width.get_value_as_int();
        pConfig->codeboxWidthPixels = radiobutton_codebox_pixels.get_active();
        pConfig->codeboxHeight = spinbutton_height.get_value();
        pConfig->codeboxLineNum = checkbutton_codebox_linenumbers.get_active();
        pConfig->codeboxMatchBra = checkbutton_codebox_matchbrackets.get_active();
        if (radiobutton_plain_text.get_active()) {
            pConfig->codeboxSynHighl = CtConst::PLAIN_TEXT_ID;
        }
        else {
            pConfig->codeboxSynHighl = button_prog_lang.get_label();
        }
        return true;
    }
    return false;
}

CtDialogs::TableHandleResp CtDialogs::table_handle_dialog(CtMainWin* pCtMainWin,
                                                          const Glib::ustring& title,
                                                          const bool is_insert,
                                                          bool& is_light)
{
    Gtk::Dialog dialog{title,
                       *pCtMainWin,
                       Gtk::DialogFlags::DIALOG_MODAL | Gtk::DialogFlags::DIALOG_DESTROY_WITH_PARENT};
    dialog.set_transient_for(*pCtMainWin);

    (void)CtMiscUtil::dialog_add_button(&dialog, _("Cancel"), Gtk::RESPONSE_REJECT, "ct_cancel");
    (void)CtMiscUtil::dialog_add_button(&dialog, _("OK"), Gtk::RESPONSE_ACCEPT, "ct_done", true/*isDefault*/);

    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    dialog.set_default_size(300, -1);

    auto pCtConfig = pCtMainWin->get_ct_config();
    auto label_rows = Gtk::Label{_("Rows")};
    label_rows.set_halign(Gtk::Align::ALIGN_START);
    label_rows.set_margin_start(10);
    auto adj_rows = Gtk::Adjustment::create(pCtConfig->tableRows, 1, 10000, 1);
    auto spinbutton_rows = Gtk::SpinButton{adj_rows};
    spinbutton_rows.set_value(pCtConfig->tableRows);
    auto label_columns = Gtk::Label{_("Columns")};
    label_columns.set_halign(Gtk::Align::ALIGN_START);
    auto adj_columns = Gtk::Adjustment::create(pCtConfig->tableColumns, 1, 10000, 1);
    auto spinbutton_columns = Gtk::SpinButton{adj_columns};
    spinbutton_columns.set_value(pCtConfig->tableColumns);

    auto label_col_width = Gtk::Label{_("Default Width")};
    label_col_width.set_halign(Gtk::Align::ALIGN_START);
    auto adj_col_width = Gtk::Adjustment::create(pCtConfig->tableColWidthDefault, 1, 10000, 1);
    auto spinbutton_col_width = Gtk::SpinButton{adj_col_width};
    spinbutton_col_width.set_value(pCtConfig->tableColWidthDefault);

    auto label_size = Gtk::Label{std::string("<b>")+_("Table Size")+"</b>"};
    label_size.set_use_markup();
    label_size.set_halign(Gtk::Align::ALIGN_START);
    auto label_col = Gtk::Label{std::string("<b>")+_("Column Properties")+"</b>"};
    label_col.set_use_markup();
    label_col.set_halign(Gtk::Align::ALIGN_START);

    Gtk::Grid grid;
    grid.property_margin() = 6;
    grid.set_row_spacing(4);
    grid.set_column_spacing(8);
    grid.set_row_homogeneous(true);

    if (is_insert) {
        grid.attach(label_size,         0, 0, 2, 1);
        grid.attach(label_rows,         0, 1, 1, 1);
        grid.attach(spinbutton_rows,    1, 1, 1, 1);
        grid.attach(label_columns,      2, 1, 1, 1);
        grid.attach(spinbutton_columns, 3, 1, 1, 1);
    }
    grid.attach(label_col,             0, 2, 2, 1);
    grid.attach(label_col_width,       0, 3, 1, 1);
    grid.attach(spinbutton_col_width,  1, 3, 1, 1);

    auto checkbutton_is_light = Gtk::CheckButton(_("Lightweight Interface (much faster for large tables)"));
    checkbutton_is_light.set_active(is_light);
    auto checkbutton_table_ins_from_file = Gtk::CheckButton(_("Import from CSV File"));

    auto content_area = dialog.get_content_area();
    content_area->set_spacing(5);
    content_area->pack_start(grid);
    content_area->pack_start(checkbutton_is_light);
    if (is_insert) content_area->pack_start(checkbutton_table_ins_from_file);
    content_area->show_all();

    checkbutton_table_ins_from_file.signal_toggled().connect([&](){
        grid.set_sensitive(not checkbutton_table_ins_from_file.get_active());
    });

    if (is_insert) {
        auto f_reeval_is_light = [&](){
            checkbutton_is_light.set_active(spinbutton_rows.get_value_as_int()*spinbutton_columns.get_value_as_int() > pCtConfig->tableCellsGoLight);
        };
        spinbutton_rows.signal_value_changed().connect([f_reeval_is_light](){ f_reeval_is_light(); });
        spinbutton_columns.signal_value_changed().connect([f_reeval_is_light](){ f_reeval_is_light(); });
    }

    auto on_key_press_dialog = [&](GdkEventKey* pEventKey)->bool{
        if (GDK_KEY_Return == pEventKey->keyval or GDK_KEY_KP_Enter == pEventKey->keyval) {
            Gtk::Button* pButton = static_cast<Gtk::Button*>(dialog.get_widget_for_response(Gtk::RESPONSE_ACCEPT));
            pButton->grab_focus();
            pButton->clicked();
            return true;
        }
        if (GDK_KEY_Escape == pEventKey->keyval) {
            Gtk::Button* pButton = static_cast<Gtk::Button*>(dialog.get_widget_for_response(Gtk::RESPONSE_REJECT));
            pButton->grab_focus();
            pButton->clicked();
            return true;
        }
        return false;
    };
    dialog.signal_key_press_event().connect(on_key_press_dialog, false/*call me before other*/);

    const auto resp = dialog.run();
    if (Gtk::RESPONSE_ACCEPT == resp) {
        is_light = checkbutton_is_light.get_active();
        pCtConfig->tableRows = spinbutton_rows.get_value_as_int();
        pCtConfig->tableColumns = spinbutton_columns.get_value_as_int();
        pCtConfig->tableColWidthDefault = spinbutton_col_width.get_value_as_int();
        if (checkbutton_table_ins_from_file.get_active()) {
            return TableHandleResp::OkFromFile;
        }
        return TableHandleResp::Ok;
    }
    return TableHandleResp::Cancel;
}
#else
Glib::ustring CtDialogs::latex_handle_dialog(CtMainWin* pCtMainWin,
                                             const Glib::ustring& latex_text)
{
    CtTextView textView{pCtMainWin};
    Glib::RefPtr<Gtk::TextBuffer> rBuffer = textView.get_buffer();
    rBuffer->set_text(latex_text);
    textView.setup_for_syntax("latex");
    pCtMainWin->apply_syntax_highlighting(rBuffer, "latex", false/*forceReApply*/);

    Gtk::ScrolledWindow scrolledwindow;
    scrolledwindow.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scrolledwindow.set_child(textView.mm());

    Gtk::Dialog dialog{_("Latex Text"), *pCtMainWin, true/*modal*/, true/*use_header_bar*/};
    dialog.add_button(_("Cancel"), Gtk::ResponseType::REJECT);
    dialog.add_button(_("OK"), Gtk::ResponseType::ACCEPT);
    dialog.set_default_response(Gtk::ResponseType::ACCEPT);
    dialog.set_default_size(400, 250);

    Gtk::Box hbox{Gtk::Orientation::HORIZONTAL, 2};
    Gtk::Box vbox{Gtk::Orientation::VERTICAL, 2};
    hbox.append(scrolledwindow);
    hbox.append(vbox);

    auto pCtConfig = pCtMainWin->get_ct_config();
    Gtk::Label label_latex_size{_("Image Size dpi")};
    Glib::RefPtr<Gtk::Adjustment> adj_latex_size = Gtk::Adjustment::create(pCtConfig->latexSizeDpi, 1, 10000, 10);
    Gtk::SpinButton spinbutton_latex_size{adj_latex_size};
    Gtk::Button button_latex_tutorial{_("Tutorial")};
    Gtk::Button button_latex_reference{_("Reference")};
    button_latex_tutorial.set_tooltip_text(_("LaTeX Math and Equations Tutorial"));
    button_latex_reference.set_tooltip_text(_("LaTeX Math Symbols Reference"));
    vbox.append(button_latex_tutorial);
    vbox.append(button_latex_reference);
    vbox.append(label_latex_size);
    vbox.append(spinbutton_latex_size);

    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->set_spacing(5);
    Glib::ustring error_msg = CtImageLatex::getRenderingErrorMessage(&latex_text);
    if (not error_msg.empty()) {
        Gtk::Label label_error_msg{error_msg};
        label_error_msg.set_use_markup(true);
        pContentArea->append(label_error_msg);
        pContentArea->append(hbox);
        spinbutton_latex_size.signal_value_changed().connect([pCtMainWin, &spinbutton_latex_size]() {
            pCtMainWin->get_ct_config()->latexSizeDpi = spinbutton_latex_size.get_value_as_int();
        });
        button_latex_tutorial.signal_clicked().connect([]() {
            fs::open_weblink("https://latex-tutorial.com/tutorials/amsmath/");
        });
        button_latex_reference.signal_clicked().connect([]() {
            fs::open_weblink("https://latex-tutorial.com/symbols/math-symbols/");
        });
        return Gtk::ResponseType::ACCEPT == _run_dialog_blocking(dialog) ? rBuffer->get_text() : "";
    }

    pContentArea->append(hbox);
    spinbutton_latex_size.signal_value_changed().connect([pCtMainWin, &spinbutton_latex_size]() {
        pCtMainWin->get_ct_config()->latexSizeDpi = spinbutton_latex_size.get_value_as_int();
    });
    button_latex_tutorial.signal_clicked().connect([]() {
        fs::open_weblink("https://latex-tutorial.com/tutorials/amsmath/");
    });
    button_latex_reference.signal_clicked().connect([]() {
        fs::open_weblink("https://latex-tutorial.com/symbols/math-symbols/");
    });
    return Gtk::ResponseType::ACCEPT == _run_dialog_blocking(dialog) ? rBuffer->get_text() : "";
}

Glib::RefPtr<Gdk::Pixbuf> CtDialogs::image_handle_dialog(Gtk::Window& parent_win,
                                                         Glib::RefPtr<Gdk::Pixbuf> rOriginalPixbuf)
{
    int width = rOriginalPixbuf->get_width();
    int height = rOriginalPixbuf->get_height();
    double image_w_h_ratio = static_cast<double>(width)/height;

    Gtk::Dialog dialog{_("Image Properties"), parent_win, true/*modal*/, true/*use_header_bar*/};
    dialog.add_button(_("Cancel"), Gtk::ResponseType::REJECT);
    dialog.add_button(_("OK"), Gtk::ResponseType::ACCEPT);
    dialog.set_default_response(Gtk::ResponseType::ACCEPT);
    dialog.set_default_size(600, 500);

    Gtk::Button button_rotate_90_ccw;
    button_rotate_90_ccw.set_icon_name("ct_rotate-left");
    button_rotate_90_ccw.set_tooltip_text(_("Rotate Left"));
    Gtk::Button button_rotate_90_cw;
    button_rotate_90_cw.set_icon_name("ct_rotate-right");
    button_rotate_90_cw.set_tooltip_text(_("Rotate Right"));

    CropImageGtk4 image{rOriginalPixbuf};
    image.widget().set_hexpand(false);
    image.widget().set_vexpand(false);

    Gtk::ScrolledWindow scrolledwindow;
    scrolledwindow.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scrolledwindow.set_hexpand(true);
    scrolledwindow.set_vexpand(true);
    scrolledwindow.set_child(image.widget());

    Gtk::Box hbox_1{Gtk::Orientation::HORIZONTAL, 2};
    hbox_1.set_hexpand(true);
    hbox_1.set_vexpand(true);
    hbox_1.append(button_rotate_90_ccw);
    hbox_1.append(scrolledwindow);
    hbox_1.append(button_rotate_90_cw);

    Gtk::Button button_flip_horizontal;
    button_flip_horizontal.set_icon_name("ct_flip-horizontal");
    button_flip_horizontal.set_tooltip_text(_("Flip Horizontally"));
    Gtk::Button button_crop;
    button_crop.set_icon_name("ct_edit_cut");
    button_crop.set_tooltip_text(_("In order to crop the image, select the area with the mouse before clicking OK"));
    Gtk::Button button_flip_vertical;
    button_flip_vertical.set_icon_name("ct_flip-vertical");
    button_flip_vertical.set_tooltip_text(_("Flip Vertically"));
    Gtk::Box hbox_2{Gtk::Orientation::HORIZONTAL, 2};
    hbox_2.append(button_flip_horizontal);
    hbox_2.append(button_crop);
    hbox_2.append(button_flip_vertical);

    Gtk::Label label_width{_("Width")};
    Glib::RefPtr<Gtk::Adjustment> rAdj_width = Gtk::Adjustment::create(width, 1, 10000, 1);
    Gtk::SpinButton spinbutton_width{rAdj_width};
    Gtk::Label label_height{_("Height")};
    Glib::RefPtr<Gtk::Adjustment> rAdj_height = Gtk::Adjustment::create(height, 1, 10000, 1);
    Gtk::SpinButton spinbutton_height{rAdj_height};
    Gtk::Box hbox_3{Gtk::Orientation::HORIZONTAL, 5};
    hbox_3.append(label_width);
    hbox_3.append(spinbutton_width);
    hbox_3.append(label_height);
    hbox_3.append(spinbutton_height);

    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->set_spacing(6);
    pContentArea->append(hbox_1);
    pContentArea->append(hbox_2);
    pContentArea->append(hbox_3);

    bool stop_update = false;
    auto image_load_into_dialog = [&]() {
        stop_update = true;
        spinbutton_width.set_value(width);
        spinbutton_height.set_value(height);
        Glib::RefPtr<Gdk::Pixbuf> rPixbuf;
        if (width <= 900 && height <= 600) {
            rPixbuf = rOriginalPixbuf->scale_simple(width, height, Gdk::InterpType::BILINEAR);
        }
        else if (width > 900) {
            const int img_parms_width = 900;
            const int img_parms_height = static_cast<int>(img_parms_width / image_w_h_ratio);
            rPixbuf = rOriginalPixbuf->scale_simple(img_parms_width, img_parms_height, Gdk::InterpType::BILINEAR);
        }
        else {
            const int img_parms_height = 600;
            const int img_parms_width = static_cast<int>(img_parms_height * image_w_h_ratio);
            rPixbuf = rOriginalPixbuf->scale_simple(img_parms_width, img_parms_height, Gdk::InterpType::BILINEAR);
        }
        image.set(rPixbuf);
        stop_update = false;
    };

    button_rotate_90_cw.signal_clicked().connect([&]() {
        rOriginalPixbuf = rOriginalPixbuf->rotate_simple(Gdk::Pixbuf::Rotation::CLOCKWISE);
        image_w_h_ratio = 1./image_w_h_ratio;
        std::swap(width, height);
        image_load_into_dialog();
    });
    button_rotate_90_ccw.signal_clicked().connect([&]() {
        rOriginalPixbuf = rOriginalPixbuf->rotate_simple(Gdk::Pixbuf::Rotation::COUNTERCLOCKWISE);
        image_w_h_ratio = 1./image_w_h_ratio;
        std::swap(width, height);
        image_load_into_dialog();
    });
    button_flip_horizontal.signal_clicked().connect([&]() {
        rOriginalPixbuf = rOriginalPixbuf->flip(true);
        image_load_into_dialog();
    });
    button_crop.signal_clicked().connect([&]() {
        CtDialogs::info_dialog(_("In order to crop the image, select the area with the mouse before clicking OK"), dialog);
    });
    button_flip_vertical.signal_clicked().connect([&]() {
        rOriginalPixbuf = rOriginalPixbuf->flip(false);
        image_load_into_dialog();
    });
    spinbutton_width.signal_value_changed().connect([&]() {
        if (stop_update) return;
        width = spinbutton_width.get_value_as_int();
        height = static_cast<int>(width/image_w_h_ratio);
        image_load_into_dialog();
    });
    spinbutton_height.signal_value_changed().connect([&]() {
        if (stop_update) return;
        height = spinbutton_height.get_value_as_int();
        width = static_cast<int>(height*image_w_h_ratio);
        image_load_into_dialog();
    });

    image_load_into_dialog();
    if (Gtk::ResponseType::ACCEPT == _run_dialog_blocking(dialog)) {
        double x, y, w, h;
        image.get_crop(width, height, &x, &y, &w, &h);
        auto scaled_pixbuf = rOriginalPixbuf->scale_simple(width, height, Gdk::InterpType::BILINEAR);
        const int crop_x = std::max(0, std::min(static_cast<int>(std::round(x)), width - 1));
        const int crop_y = std::max(0, std::min(static_cast<int>(std::round(y)), height - 1));
        const int crop_w = std::max(1, std::min(static_cast<int>(std::round(w)), width - crop_x));
        const int crop_h = std::max(1, std::min(static_cast<int>(std::round(h)), height - crop_y));
        return Gdk::Pixbuf::create_subpixbuf(scaled_pixbuf, crop_x, crop_y, crop_w, crop_h)->copy();
    }
    return Glib::RefPtr<Gdk::Pixbuf>{};
}

bool CtDialogs::codeboxhandle_dialog(CtMainWin* pCtMainWin,
                                     const Glib::ustring& title)
{
    Gtk::Dialog dialog{title, *pCtMainWin, true/*modal*/, true/*use_header_bar*/};
    dialog.add_button(_("Cancel"), Gtk::ResponseType::REJECT);
    dialog.add_button(_("OK"), Gtk::ResponseType::ACCEPT);
    dialog.set_default_response(Gtk::ResponseType::ACCEPT);
    dialog.set_default_size(300, -1);

    CtConfig* pConfig = pCtMainWin->get_ct_config();

    Gtk::Button button_prog_lang;
    const Glib::ustring syntax_hl_id = pConfig->codeboxSynHighl != CtConst::PLAIN_TEXT_ID ? pConfig->codeboxSynHighl : pConfig->autoSynHighl;
    button_prog_lang.set_label(syntax_hl_id);

    Gtk::CheckButton radiobutton_plain_text{_("Plain Text")};
    Gtk::CheckButton radiobutton_auto_syntax_highl{_("Automatic Syntax Highlighting")};
    radiobutton_auto_syntax_highl.set_group(radiobutton_plain_text);
    if (pConfig->codeboxSynHighl == CtConst::PLAIN_TEXT_ID) {
        radiobutton_plain_text.set_active(true);
        button_prog_lang.set_sensitive(false);
    }
    else {
        radiobutton_auto_syntax_highl.set_active(true);
    }

    Gtk::Box type_vbox{Gtk::Orientation::VERTICAL};
    type_vbox.append(radiobutton_plain_text);
    type_vbox.append(radiobutton_auto_syntax_highl);
    type_vbox.append(button_prog_lang);
    Gtk::Frame type_frame{Glib::ustring{"<b>"} + _("Type") + "</b>"};
    dynamic_cast<Gtk::Label*>(type_frame.get_label_widget())->set_use_markup(true);
    type_frame.set_child(type_vbox);

    Gtk::Label label_width{_("Width")};
    Glib::RefPtr<Gtk::Adjustment> rAdj_width = Gtk::Adjustment::create(pConfig->codeboxWidth, 1, 10000);
    Gtk::SpinButton spinbutton_width{rAdj_width};
    spinbutton_width.set_value(pConfig->codeboxWidth);
    Gtk::Label label_height{_("Height")};
    Glib::RefPtr<Gtk::Adjustment> rAdj_height = Gtk::Adjustment::create(pConfig->codeboxHeight, 1, 10000);
    Gtk::SpinButton spinbutton_height{rAdj_height};
    spinbutton_height.set_value(pConfig->codeboxHeight);

    Gtk::CheckButton radiobutton_codebox_pixels{_("pixels")};
    Gtk::CheckButton radiobutton_codebox_percent{"%"};
    radiobutton_codebox_percent.set_group(radiobutton_codebox_pixels);
    radiobutton_codebox_pixels.set_active(pConfig->codeboxWidthPixels);
    radiobutton_codebox_percent.set_active(!pConfig->codeboxWidthPixels);

    Gtk::Box vbox_pix_perc{Gtk::Orientation::VERTICAL};
    vbox_pix_perc.append(radiobutton_codebox_pixels);
    vbox_pix_perc.append(radiobutton_codebox_percent);
    Gtk::Box hbox_width{Gtk::Orientation::HORIZONTAL, 5};
    hbox_width.append(label_width);
    hbox_width.append(spinbutton_width);
    hbox_width.append(vbox_pix_perc);
    Gtk::Box hbox_height{Gtk::Orientation::HORIZONTAL, 5};
    hbox_height.append(label_height);
    hbox_height.append(spinbutton_height);
    Gtk::Box vbox_size{Gtk::Orientation::VERTICAL};
    vbox_size.append(hbox_width);
    vbox_size.append(hbox_height);
    CtMiscUtil::set_widget_margins(vbox_size, 0, 6, 6, 6);

    Gtk::Frame size_frame{Glib::ustring{"<b>"} + _("Size") + "</b>"};
    dynamic_cast<Gtk::Label*>(size_frame.get_label_widget())->set_use_markup(true);
    size_frame.set_child(vbox_size);

    Gtk::CheckButton checkbutton_codebox_linenumbers{_("Show Line Numbers")};
    checkbutton_codebox_linenumbers.set_active(pConfig->codeboxLineNum);
    Gtk::CheckButton checkbutton_codebox_matchbrackets{_("Highlight Matching Brackets")};
    checkbutton_codebox_matchbrackets.set_active(pConfig->codeboxMatchBra);
    Gtk::Box vbox_options{Gtk::Orientation::VERTICAL};
    vbox_options.append(checkbutton_codebox_linenumbers);
    vbox_options.append(checkbutton_codebox_matchbrackets);
    CtMiscUtil::set_widget_margins(vbox_options, 6, 6, 6, 6);

    Gtk::Frame options_frame{Glib::ustring{"<b>"} + _("Options") + "</b>"};
    dynamic_cast<Gtk::Label*>(options_frame.get_label_widget())->set_use_markup(true);
    options_frame.set_child(vbox_options);

    Gtk::Box* pContentArea = dialog.get_content_area();
    pContentArea->set_spacing(5);
    pContentArea->append(type_frame);
    pContentArea->append(size_frame);
    pContentArea->append(options_frame);

    button_prog_lang.signal_clicked().connect([&button_prog_lang, &dialog, pCtMainWin, pConfig]() {
        auto rItemStore = CtChooseDialogListStore::create();
        unsigned pathSelectIdx{0};
        unsigned pathCurrIdx{0};
        const auto currSyntaxHighl = button_prog_lang.get_label();
        const gchar* const* pLanguageIDs = gtk_source_language_manager_get_language_ids(pCtMainWin->get_language_manager());
        for (auto pLang = pLanguageIDs; *pLang; ++pLang) {
            rItemStore->add_row(pCtMainWin->get_code_icon_name(*pLang), "", *pLang);
            if (*pLang == currSyntaxHighl) {
                pathSelectIdx = pathCurrIdx;
            }
            ++pathCurrIdx;
        }
        Gtk::TreeModel::iterator res = CtDialogs::choose_item_dialog(dialog,
                                                                      _("Automatic Syntax Highlighting"),
                                                                      rItemStore,
                                                                      nullptr,
                                                                      std::to_string(pathSelectIdx),
                                                                      std::make_pair(200, pConfig->winRect[3]));
        if (res) {
            button_prog_lang.set_label(res->get_value(rItemStore->columns.desc));
        }
    });
    radiobutton_auto_syntax_highl.signal_toggled().connect([&radiobutton_auto_syntax_highl, &button_prog_lang]() {
        button_prog_lang.set_sensitive(radiobutton_auto_syntax_highl.get_active());
    });
    radiobutton_codebox_pixels.signal_toggled().connect([&radiobutton_codebox_pixels, &spinbutton_width]() {
        if (radiobutton_codebox_pixels.get_active()) {
            spinbutton_width.set_value(700);
        }
        else if (spinbutton_width.get_value() > 100) {
            spinbutton_width.set_value(90);
        }
    });

    if (_run_dialog_blocking(dialog) == Gtk::ResponseType::ACCEPT) {
        pConfig->codeboxWidth = spinbutton_width.get_value_as_int();
        pConfig->codeboxWidthPixels = radiobutton_codebox_pixels.get_active();
        pConfig->codeboxHeight = spinbutton_height.get_value();
        pConfig->codeboxLineNum = checkbutton_codebox_linenumbers.get_active();
        pConfig->codeboxMatchBra = checkbutton_codebox_matchbrackets.get_active();
        if (radiobutton_plain_text.get_active()) {
            pConfig->codeboxSynHighl = CtConst::PLAIN_TEXT_ID;
        }
        else {
            pConfig->codeboxSynHighl = button_prog_lang.get_label();
        }
        return true;
    }
    return false;
}

CtDialogs::TableHandleResp CtDialogs::table_handle_dialog(CtMainWin* pCtMainWin,
                                                          const Glib::ustring& title,
                                                          const bool is_insert,
                                                          bool& is_light)
{
    Gtk::Dialog dialog{title, *pCtMainWin, true/*modal*/, true/*use_header_bar*/};
    dialog.add_button(_("Cancel"), Gtk::ResponseType::REJECT);
    dialog.add_button(_("OK"), Gtk::ResponseType::ACCEPT);
    dialog.set_default_response(Gtk::ResponseType::ACCEPT);
    dialog.set_default_size(300, -1);

    auto pCtConfig = pCtMainWin->get_ct_config();
    Gtk::Label label_rows{_("Rows")};
    label_rows.set_halign(Gtk::Align::START);
    label_rows.set_margin_start(10);
    auto adj_rows = Gtk::Adjustment::create(pCtConfig->tableRows, 1, 10000, 1);
    Gtk::SpinButton spinbutton_rows{adj_rows};
    spinbutton_rows.set_value(pCtConfig->tableRows);
    Gtk::Label label_columns{_("Columns")};
    label_columns.set_halign(Gtk::Align::START);
    auto adj_columns = Gtk::Adjustment::create(pCtConfig->tableColumns, 1, 10000, 1);
    Gtk::SpinButton spinbutton_columns{adj_columns};
    spinbutton_columns.set_value(pCtConfig->tableColumns);

    Gtk::Label label_col_width{_("Default Width")};
    label_col_width.set_halign(Gtk::Align::START);
    auto adj_col_width = Gtk::Adjustment::create(pCtConfig->tableColWidthDefault, 1, 10000, 1);
    Gtk::SpinButton spinbutton_col_width{adj_col_width};
    spinbutton_col_width.set_value(pCtConfig->tableColWidthDefault);

    Gtk::Label label_size{std::string{"<b>"} + _("Table Size") + "</b>"};
    label_size.set_use_markup(true);
    label_size.set_halign(Gtk::Align::START);
    Gtk::Label label_col{std::string{"<b>"} + _("Column Properties") + "</b>"};
    label_col.set_use_markup(true);
    label_col.set_halign(Gtk::Align::START);

    Gtk::Grid grid;
    grid.set_margin_top(6);
    grid.set_margin_bottom(6);
    grid.set_margin_start(6);
    grid.set_margin_end(6);
    grid.set_row_spacing(4);
    grid.set_column_spacing(8);
    grid.set_row_homogeneous(true);

    if (is_insert) {
        grid.attach(label_size,         0, 0, 2, 1);
        grid.attach(label_rows,         0, 1, 1, 1);
        grid.attach(spinbutton_rows,    1, 1, 1, 1);
        grid.attach(label_columns,      2, 1, 1, 1);
        grid.attach(spinbutton_columns, 3, 1, 1, 1);
    }
    grid.attach(label_col,             0, 2, 2, 1);
    grid.attach(label_col_width,       0, 3, 1, 1);
    grid.attach(spinbutton_col_width,  1, 3, 1, 1);

    Gtk::CheckButton checkbutton_is_light{_("Lightweight Interface (much faster for large tables)")};
    checkbutton_is_light.set_active(is_light);
    Gtk::CheckButton checkbutton_table_ins_from_file{_("Import from CSV File")};

    auto content_area = dialog.get_content_area();
    content_area->set_spacing(5);
    content_area->append(grid);
    content_area->append(checkbutton_is_light);
    if (is_insert) {
        content_area->append(checkbutton_table_ins_from_file);
    }

    checkbutton_table_ins_from_file.signal_toggled().connect([&]() {
        grid.set_sensitive(not checkbutton_table_ins_from_file.get_active());
    });

    if (is_insert) {
        auto f_reeval_is_light = [&]() {
            checkbutton_is_light.set_active(spinbutton_rows.get_value_as_int() * spinbutton_columns.get_value_as_int() > pCtConfig->tableCellsGoLight);
        };
        spinbutton_rows.signal_value_changed().connect([f_reeval_is_light]() { f_reeval_is_light(); });
        spinbutton_columns.signal_value_changed().connect([f_reeval_is_light]() { f_reeval_is_light(); });
    }

    if (_run_dialog_blocking(dialog) == Gtk::ResponseType::ACCEPT) {
        is_light = checkbutton_is_light.get_active();
        pCtConfig->tableRows = spinbutton_rows.get_value_as_int();
        pCtConfig->tableColumns = spinbutton_columns.get_value_as_int();
        pCtConfig->tableColWidthDefault = spinbutton_col_width.get_value_as_int();
        if (checkbutton_table_ins_from_file.get_active()) {
            return TableHandleResp::OkFromFile;
        }
        return TableHandleResp::Ok;
    }
    return TableHandleResp::Cancel;
}
#endif
