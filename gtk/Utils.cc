/*
 * This file Copyright (C) 2008-2021 Mnemosyne LLC
 *
 * It may be used under the GNU GPL versions 2 or 3
 * or any future license endorsed by Mnemosyne LLC.
 *
 */

#include <array>
#include <ctype.h> /* isxdigit() */
#include <errno.h>
#include <limits.h> /* INT_MAX */
#include <stdarg.h>
#include <string.h> /* strchr(), strrchr(), strlen(), strstr() */

#include <giomm.h> /* g_file_trash() */
#include <glibmm/i18n.h>

#include <libtransmission/transmission.h> /* TR_RATIO_NA, TR_RATIO_INF */
#include <libtransmission/error.h>
#include <libtransmission/utils.h> /* tr_strratio() */
#include <libtransmission/web-utils.h>
#include <libtransmission/version.h> /* SHORT_VERSION_STRING */

#include "HigWorkarea.h"
#include "Prefs.h"
#include "PrefsDialog.h"
#include "Session.h"
#include "Utils.h"

/***
****  UNITS
***/

int const mem_K = 1024;
char const* mem_K_str = N_("KiB");
char const* mem_M_str = N_("MiB");
char const* mem_G_str = N_("GiB");
char const* mem_T_str = N_("TiB");

int const disk_K = 1000;
char const* disk_K_str = N_("kB");
char const* disk_M_str = N_("MB");
char const* disk_G_str = N_("GB");
char const* disk_T_str = N_("TB");

int const speed_K = 1000;
char const* speed_K_str = N_("kB/s");
char const* speed_M_str = N_("MB/s");
char const* speed_G_str = N_("GB/s");
char const* speed_T_str = N_("TB/s");

/***
****
***/

Glib::ustring gtr_get_unicode_string(int i)
{
    switch (i)
    {
    case GTR_UNICODE_UP:
        return "\xE2\x96\xB4";

    case GTR_UNICODE_DOWN:
        return "\xE2\x96\xBE";

    case GTR_UNICODE_INF:
        return "\xE2\x88\x9E";

    case GTR_UNICODE_BULLET:
        return "\xE2\x88\x99";

    default:
        return "err";
    }
}

Glib::ustring tr_strlratio(double ratio)
{
    std::array<char, 64> buf = {};
    return tr_strratio(buf.data(), buf.size(), ratio, gtr_get_unicode_string(GTR_UNICODE_INF).c_str());
}

Glib::ustring tr_strlpercent(double x)
{
    std::array<char, 64> buf = {};
    return tr_strpercent(buf.data(), x, buf.size());
}

Glib::ustring tr_strlsize(guint64 bytes)
{
    if (bytes == 0)
    {
        return Q_("None");
    }

    std::array<char, 64> buf = {};
    return tr_formatter_size_B(buf.data(), bytes, buf.size());
}

Glib::ustring tr_strltime(time_t seconds)
{
    if (seconds < 0)
    {
        seconds = 0;
    }

    int const days = (int)(seconds / 86400);
    int const hours = (seconds % 86400) / 3600;
    int const minutes = (seconds % 3600) / 60;
    seconds = (seconds % 3600) % 60;

    auto const d = gtr_sprintf(ngettext("%'d day", "%'d days", days), days);
    auto const h = gtr_sprintf(ngettext("%'d hour", "%'d hours", hours), hours);
    auto const m = gtr_sprintf(ngettext("%'d minute", "%'d minutes", minutes), minutes);
    auto const s = gtr_sprintf(ngettext("%'d second", "%'d seconds", (int)seconds), (int)seconds);

    if (days != 0)
    {
        return (days >= 4 || hours == 0) ? d : gtr_sprintf("%s, %s", d, h);
    }
    else if (hours != 0)
    {
        return (hours >= 4 || minutes == 0) ? h : gtr_sprintf("%s, %s", h, m);
    }
    else if (minutes != 0)
    {
        return (minutes >= 4 || seconds == 0) ? m : gtr_sprintf("%s, %s", m, s);
    }
    else
    {
        return s;
    }
}

/* pattern-matching text; ie, legaltorrents.com */
Glib::ustring gtr_get_host_from_url(Glib::ustring const& url)
{
    Glib::ustring host;

    if (auto const pch = url.find("://"); pch != Glib::ustring::npos)
    {
        auto const hostend = url.find_first_of(":/", pch + 3);
        host = url.substr(pch + 3, hostend == Glib::ustring::npos ? hostend : (hostend - pch - 3));
    }

    if (tr_addressIsIP(host.c_str()))
    {
        return url;
    }
    else
    {
        auto const first_dot = host.find('.');
        auto const last_dot = host.rfind('.');

        if (first_dot != Glib::ustring::npos && last_dot != Glib::ustring::npos && first_dot != last_dot)
        {
            return host.substr(first_dot + 1);
        }
        else
        {
            return host;
        }
    }
}

namespace
{

bool gtr_is_supported_url(Glib::ustring const& str)
{
    return !str.empty() &&
        (Glib::str_has_prefix(str, "ftp://") || Glib::str_has_prefix(str, "http://") || Glib::str_has_prefix(str, "https://"));
}

} // namespace

bool gtr_is_magnet_link(Glib::ustring const& str)
{
    return !str.empty() && Glib::str_has_prefix(str, "magnet:?");
}

bool gtr_is_hex_hashcode(std::string const& str)
{
    if (str.size() != 40)
    {
        return false;
    }

    for (int i = 0; i < 40; ++i)
    {
        if (!isxdigit(str[i]))
        {
            return false;
        }
    }

    return true;
}

namespace
{

Gtk::Window* getWindow(Gtk::Widget* w)
{
    if (w == nullptr)
    {
        return nullptr;
    }

    if (auto* const window = dynamic_cast<Gtk::Window*>(w); window != nullptr)
    {
        return window;
    }

    return static_cast<Gtk::Window*>(w->get_ancestor(Gtk::Window::get_type()));
}

} // namespace

void gtr_add_torrent_error_dialog(Gtk::Widget& child, int err, tr_torrent* duplicate_torrent, std::string const& filename)
{
    Glib::ustring secondary;
    auto* win = getWindow(&child);

    if (err == TR_PARSE_ERR)
    {
        secondary = gtr_sprintf(_("The torrent file \"%s\" contains invalid data."), filename);
    }
    else if (err == TR_PARSE_DUPLICATE)
    {
        secondary = gtr_sprintf(
            _("The torrent file \"%s\" is already in use by \"%s.\""),
            filename,
            tr_torrentName(duplicate_torrent));
    }
    else
    {
        secondary = gtr_sprintf(_("The torrent file \"%s\" encountered an unknown error."), filename);
    }

    auto* w = new Gtk::MessageDialog(*win, _("Error opening torrent"), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    w->set_secondary_text(secondary);
    w->signal_response().connect([w](int /*response*/) { delete w; });
    w->show_all();
}

/* pop up the context menu if a user right-clicks.
   if the row they right-click on isn't selected, select it. */
bool on_tree_view_button_pressed(
    Gtk::TreeView* view,
    GdkEventButton* event,
    std::function<void(GdkEventButton*)> const& callback)
{
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        Gtk::TreeModel::Path path;
        auto const selection = view->get_selection();

        if (view->get_path_at_pos((int)event->x, (int)event->y, path))
        {
            if (!selection->is_selected(path))
            {
                selection->unselect_all();
                selection->select(path);
            }
        }

        if (callback)
        {
            callback(event);
        }

        return true;
    }

    return false;
}

/* if the user clicked in an empty area of the list,
 * clear all the selections. */
bool on_tree_view_button_released(Gtk::TreeView* view, GdkEventButton* event)
{
    Gtk::TreeModel::Path path;

    if (!view->get_path_at_pos((int)event->x, (int)event->y, path))
    {
        view->get_selection()->unselect_all();
    }

    return false;
}

bool gtr_file_trash_or_remove(std::string const& filename, tr_error** error)
{
    bool trashed = false;
    bool result = true;

    g_return_val_if_fail(!filename.empty(), false);

    auto const file = Gio::File::create_for_path(filename);

    if (gtr_pref_flag_get(TR_KEY_trash_can_enabled))
    {
        try
        {
            trashed = file->trash();
        }
        catch (Glib::Error const& e)
        {
            g_message("Unable to trash file \"%s\": %s", filename.c_str(), e.what().c_str());
            tr_error_set_literal(error, e.code(), e.what().c_str());
        }
    }

    if (!trashed)
    {
        try
        {
            file->remove();
        }
        catch (Glib::Error const& e)
        {
            g_message("Unable to delete file \"%s\": %s", filename.c_str(), e.what().c_str());
            tr_error_clear(error);
            tr_error_set_literal(error, e.code(), e.what().c_str());
            result = false;
        }
    }

    return result;
}

Glib::ustring gtr_get_help_uri()
{
    static auto const uri = gtr_sprintf("https://transmissionbt.com/help/gtk/%d.%dx", MAJOR_VERSION, MINOR_VERSION / 10);
    return uri;
}

void gtr_open_file(std::string const& path)
{
    gtr_open_uri(Gio::File::create_for_path(path)->get_uri());
}

void gtr_open_uri(Glib::ustring const& uri)
{
    if (!uri.empty())
    {
        bool opened = false;

        if (!opened)
        {
            try
            {
                opened = Gio::AppInfo::launch_default_for_uri(uri);
            }
            catch (Glib::Error const&)
            {
            }
        }

        if (!opened)
        {
            try
            {
                Glib::spawn_async({}, std::vector<std::string>{ "xdg-open", uri }, Glib::SPAWN_SEARCH_PATH);
                opened = true;
            }
            catch (Glib::SpawnError const&)
            {
            }
        }

        if (!opened)
        {
            g_message("Unable to open \"%s\"", uri.c_str());
        }
    }
}

/***
****
***/

namespace
{

class EnumComboModelColumns : public Gtk::TreeModelColumnRecord
{
public:
    EnumComboModelColumns()
    {
        add(value);
        add(label);
    }

    Gtk::TreeModelColumn<int> value;
    Gtk::TreeModelColumn<Glib::ustring> label;
};

EnumComboModelColumns const enum_combo_cols;

} // namespace

void gtr_combo_box_set_active_enum(Gtk::ComboBox& combo_box, int value)
{
    auto const& column = enum_combo_cols.value;

    /* do the value and current value match? */
    if (auto const iter = combo_box.get_active(); iter)
    {
        if (iter->get_value(column) == value)
        {
            return;
        }
    }

    /* find the one to select */
    for (auto const& row : combo_box.get_model()->children())
    {
        if (row.get_value(column) == value)
        {
            combo_box.set_active(row);
            return;
        }
    }
}

Gtk::ComboBox* gtr_combo_box_new_enum(std::vector<std::pair<Glib::ustring, int>> const& items)
{
    auto store = Gtk::ListStore::create(enum_combo_cols);

    for (auto const& item : items)
    {
        auto const iter = store->append();
        (*iter)[enum_combo_cols.value] = item.second;
        (*iter)[enum_combo_cols.label] = item.first;
    }

    auto w = Gtk::make_managed<Gtk::ComboBox>(static_cast<Glib::RefPtr<Gtk::TreeModel> const&>(store));
    auto* r = Gtk::make_managed<Gtk::CellRendererText>();
    w->pack_start(*r, true);
    w->add_attribute(r->property_text(), enum_combo_cols.label);

    return w;
}

int gtr_combo_box_get_active_enum(Gtk::ComboBox const& combo_box)
{
    int value = 0;

    if (auto const iter = combo_box.get_active(); iter)
    {
        iter->get_value(0, value);
    }

    return value;
}

Gtk::ComboBox* gtr_priority_combo_new()
{
    return gtr_combo_box_new_enum({
        { _("High"), TR_PRI_HIGH },
        { _("Normal"), TR_PRI_NORMAL },
        { _("Low"), TR_PRI_LOW },
    });
}

/***
****
***/

#define GTR_CHILD_HIDDEN "gtr-child-hidden"

void gtr_widget_set_visible(Gtk::Widget& w, bool b)
{
    /* toggle the transient children, too */
    if (auto* const window = dynamic_cast<Gtk::Window*>(&w); window != nullptr)
    {
        for (auto* const l : Gtk::Window::list_toplevels())
        {
            if (l->get_transient_for() != window)
            {
                continue;
            }

            if (l->get_visible() == b)
            {
                continue;
            }

            if (b && l->get_data(GTR_CHILD_HIDDEN) != nullptr)
            {
                l->steal_data(GTR_CHILD_HIDDEN);
                gtr_widget_set_visible(*l, true);
            }
            else if (!b)
            {
                l->set_data(GTR_CHILD_HIDDEN, GINT_TO_POINTER(1));
                gtr_widget_set_visible(*l, false);
            }
        }
    }

    w.set_visible(b);
}

void gtr_dialog_set_content(Gtk::Dialog& dialog, Gtk::Widget& content)
{
    auto* vbox = dialog.get_content_area();
    vbox->pack_start(content, true, true, 0);
    content.show_all();
}

/***
****
***/

void gtr_unrecognized_url_dialog(Gtk::Widget& parent, Glib::ustring const& url)
{
    char const* xt = "xt=urn:btih";

    auto* window = getWindow(&parent);

    Glib::ustring gstr;

    auto* w = new Gtk::MessageDialog(*window, _("Unrecognized URL"), false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);

    gstr += gtr_sprintf(_("Transmission doesn't know how to use \"%s\""), url);

    if (gtr_is_magnet_link(url) && url.find(xt) == Glib::ustring::npos)
    {
        gstr += "\n \n";
        gstr += gtr_sprintf(
            _("This magnet link appears to be intended for something other than BitTorrent. "
              "BitTorrent magnet links have a section containing \"%s\"."),
            xt);
    }

    w->set_secondary_text(gstr);
    w->signal_response().connect([w](int /*response*/) { delete w; });
    w->show();
}

/***
****
***/

void gtr_paste_clipboard_url_into_entry(Gtk::Entry& e)
{
    Glib::ustring const text[] = {
        gtr_str_strip(Gtk::Clipboard::get(GDK_SELECTION_PRIMARY)->wait_for_text()),
        gtr_str_strip(Gtk::Clipboard::get(GDK_SELECTION_CLIPBOARD)->wait_for_text()),
    };

    for (auto const& s : text)
    {
        if (!s.empty() && (gtr_is_supported_url(s) || gtr_is_magnet_link(s) || gtr_is_hex_hashcode(s)))
        {
            e.set_text(s);
            break;
        }
    }
}

/***
****
***/

void gtr_label_set_text(Gtk::Label& lb, Glib::ustring const& newstr)
{
    if (lb.get_text() != newstr)
    {
        lb.set_text(newstr);
    }
}
