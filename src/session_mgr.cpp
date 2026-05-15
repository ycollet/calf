/* Calf DSP Library Utility Application - calfjackhost
 * Session manager API implementation for LASH 0.5.4 and 0.6.0
 *
 * Copyright (C) 2010 Krzysztof Foltman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include "config.h"
#include <calf/session_mgr.h>
#include <string>

#if USE_LASH

#include <calf/utils.h>
#include <gtk/gtk.h>
#include <lash/lash.h>
#include <glib.h>
#include <string.h>

using namespace std;
using namespace calf_plugins;


class lash_session_manager_base: public session_manager_iface
{
protected:
    session_client_iface *client;
    int lash_source_id;
    lash_client_t *lash_client;
    bool restoring_session;
    static gboolean update_lash(void *self) { ((session_manager_iface *)self)->poll(); return TRUE; }
public:
    lash_session_manager_base(session_client_iface *_client);

    void connect(const std::string &name)
    {
        if (lash_client)
        {
            lash_source_id = g_timeout_add_full(G_PRIORITY_DEFAULT, 250, update_lash, (session_manager_iface *)this, NULL); // 4 LASH reads per second... should be enough?
        }
    }
    void disconnect();
};

lash_session_manager_base::lash_session_manager_base(session_client_iface *_client)
{
    client = _client;
    lash_source_id = 0;
    lash_client = NULL;
    restoring_session = false;
}

void lash_session_manager_base::disconnect()
{
    if (lash_source_id)
    {
        g_source_remove(lash_source_id);
        lash_source_id = 0;
    }
}

# if !USE_LASH_0_6

class old_lash_session_manager: public lash_session_manager_base
{
    lash_args_t *lash_args;

public:
    old_lash_session_manager(session_client_iface *_client, int &argc, char **&argv);
    void send_lash(LASH_Event_Type type, const std::string &data);
    virtual void connect(const std::string &name);
    virtual void disconnect();
    virtual bool is_being_restored() { return restoring_session; }
    virtual void set_jack_client_name(const std::string &name);
    virtual void poll();

};

old_lash_session_manager::old_lash_session_manager(session_client_iface *_client, int &argc, char **&argv)
: lash_session_manager_base(_client)
{
    lash_args = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "--lash-project=", 14)) {
            restoring_session = true;
            break;
        }
    }
    lash_args = lash_extract_args(&argc, &argv);
    lash_client = lash_init(lash_args, PACKAGE_NAME, LASH_Config_Data_Set, LASH_PROTOCOL(2, 0));
    if (!lash_client) {
        g_warning("Failed to create a LASH connection");
    }
}

void old_lash_session_manager::send_lash(LASH_Event_Type type, const std::string &data)
{
    lash_send_event(lash_client, lash_event_new_with_all(type, data.c_str()));
}

void old_lash_session_manager::connect(const std::string &name)
{
    if (lash_client)
    {
        send_lash(LASH_Client_Name, name);
    }
    lash_session_manager_base::connect(name);
}

void old_lash_session_manager::disconnect()
{
    lash_session_manager_base::disconnect();
    if (lash_args)
        lash_args_destroy(lash_args);
}

void old_lash_session_manager::set_jack_client_name(const std::string &name)
{
    if (lash_client)
        lash_jack_client_name(lash_client, name.c_str());
}

void old_lash_session_manager::poll()
{
    do {
        lash_event_t *event = lash_get_event(lash_client);
        if (!event)
            break;
        
        // printf("type = %d\n", lash_event_get_type(event));
        
        switch(lash_event_get_type(event)) {        
            case LASH_Save_Data_Set:
            {
                struct writer: public session_save_iface
                {
                    lash_client_t *client;
                    
                    void write_next_item(const std::string &key, const std::string &value)
                    {
                        lash_config_t *cfg = lash_config_new_with_key(key.c_str());
                        lash_config_set_value(cfg, value.c_str(), value.length());
                        lash_send_config(client, cfg);
                    }
                };
                
                writer w;
                w.client = lash_client;
                client->save(&w);
                send_lash(LASH_Save_Data_Set, "");
                break;
            }
            
            case LASH_Restore_Data_Set:
            {
                struct reader: public session_load_iface
                {
                    lash_client_t *client;
                    
                    virtual bool get_next_item(std::string &key, std::string &value) {
                        lash_config_t *cfg = lash_get_config(client);
                        
                        if(cfg) {
                            key = lash_config_get_key(cfg);
                            // printf("key = %s\n", lash_config_get_key(cfg));
                            value = string((const char *)lash_config_get_value(cfg), lash_config_get_value_size(cfg));
                            lash_config_destroy(cfg);
                            return true;
                        }
                        return false;
                    }        
                };
                
                reader r;
                r.client = lash_client;
                client->load(&r);
                send_lash(LASH_Restore_Data_Set, "");
                break;
            }
                
            case LASH_Quit:
                gtk_main_quit();
                break;
            
            default:
                g_warning("Unhandled LASH event %d (%s)", lash_event_get_type(event), lash_event_get_string(event));
                break;
        }
    } while(1);
}

session_manager_iface *calf_plugins::create_lash_session_mgr(session_client_iface *client, int &argc, char **&argv)
{
    return new old_lash_session_manager(client, argc, argv);
}

# else

class new_lash_session_manager: public lash_session_manager_base
{
    lash_args_t *lash_args;

    static bool save_data_set_cb(lash_config_handle_t *handle, void *user_data);
    static bool load_data_set_cb(lash_config_handle_t *handle, void *user_data);
    static bool quit_cb(void *user_data);

public:
    new_lash_session_manager(session_client_iface *_client)
    
    virtual void set_jack_client_name(const std::string &) {}
};

new_lash_session_manager::new_lash_session_manager(session_client_iface *_client)
: lash_session_manager_base(_client)
{
    lash_client = lash_client_open(PACKAGE_NAME, LASH_Config_Data_Set, argc, argv);
    if (!lash_client) {
        g_warning("Failed to create a LASH connection");
        return;
    }
    restoring_session = lash_client_is_being_restored(lash_client);
    lash_set_save_data_set_callback(lash_client, save_data_set_cb, this);
    lash_set_load_data_set_callback(lash_client, load_data_set_cb, this);
    lash_set_quit_callback(lash_client, quit_cb, NULL);    
}

void new_lash_session_manager::poll()
{
    lash_dispatch(lash_client);
}

bool new_lash_session_manager::save_data_set_cb(lash_config_handle_t *handle, void *user_data)
{
    struct writer: public session_save_iface
    {
        lash_config_handle_t handle;
        
        virtual bool write_next_item(const std::string &key, const std::string &value) {
            lash_config_write_raw(handle, key.c_str(), (const void *)value.data(), value.length(), LASH_TYPE_RAW);
            return true;
        }        
    };
    
    writer w;
    w.handle = handle;
    client->save(&w);
    return true;
}

bool new_lash_session_manager::load_data_set_cb(lash_config_handle_t *handle, void *user_data)
{
    struct reader: public session_load_iface
    {
        lash_config_handle_t handle;
        
        virtual bool get_next_item(std::string &key, std::string &value) {
            const char *key_cstr;
            int type;
            void *data;
            size = lash_config_read(handle, &key_cstr, &data, &type);
            if (size == -1 || type != LASH_TYPE_RAW)
                return false;
            key = key_cstr;
            value = string((const char *)data, size);
            return true;
        }        
    };
    
    reader r;
    r.handle = handle;
    client->load(&r);
    return true;
}

bool new_lash_session_manager::quit_cb(void *user_data)
{
    gtk_main_quit();
    return true;
}

session_manager_iface *calf_plugins::create_lash_session_mgr(session_client_iface *_client, int &, char **&)
{
    return new new_lash_session_manager(client);
}

# endif

#endif

#if USE_NSM

#include <lo/lo.h>
#include <glib.h>
#include <unistd.h>

using namespace std;
using namespace calf_plugins;

class nsm_session_manager : public session_manager_iface
{
    session_client_iface *client;
    lo_server server;
    lo_address nsm_addr;
    bool being_restored;
    bool got_open;
    std::string session_dir;
    std::string client_id;
    int source_id;

    static int on_reply(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int on_open(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int on_save(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    static int on_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);

    static gboolean poll_server(void *data) {
        lo_server_recv_noblock((lo_server)data, 0);
        return TRUE;
    }

public:
    nsm_session_manager(session_client_iface *_client, const char *nsm_url, const char *exe_name);
    ~nsm_session_manager();

    virtual bool is_being_restored() { return being_restored; }
    virtual void set_jack_client_name(const std::string &) {}
    virtual void connect(const std::string &name);
    virtual void poll() { if (server) lo_server_recv_noblock(server, 0); }
    virtual void disconnect();
};

nsm_session_manager::nsm_session_manager(session_client_iface *_client, const char *nsm_url, const char *exe_name)
    : client(_client), server(NULL), nsm_addr(NULL), being_restored(false), got_open(false), source_id(0)
{
    server = lo_server_new(NULL, NULL);
    if (!server) {
        g_warning("NSM: failed to create OSC server");
        return;
    }
    lo_server_add_method(server, "/reply",              NULL,  on_reply, this);
    lo_server_add_method(server, "/nsm/client/open",    "sss", on_open,  this);
    lo_server_add_method(server, "/nsm/client/save",    "",    on_save,  this);
    lo_server_add_method(server, "/nsm/client/quit",    "",    on_quit,  this);

    nsm_addr = lo_address_new_from_url(nsm_url);
    if (!nsm_addr) {
        g_warning("NSM: invalid URL: %s", nsm_url);
        return;
    }

    lo_send(nsm_addr, "/nsm/server/announce", "sssiii",
            "Calf Studio Gear",
            ":dirty:",
            exe_name,
            1, 2,
            (int)getpid());

    // Block briefly waiting for /nsm/client/open (up to 5 seconds)
    for (int i = 0; i < 500 && !got_open; i++)
        lo_server_recv_noblock(server, 10);

    if (!got_open)
        g_warning("NSM: timed out waiting for /nsm/client/open");
}

nsm_session_manager::~nsm_session_manager()
{
    disconnect();
}

void nsm_session_manager::connect(const std::string &)
{
    if (!session_dir.empty() && being_restored) {
        std::string rack_file = session_dir + G_DIR_SEPARATOR_S "rack.xml";
        char *err = client->open_file(rack_file.c_str());
        if (err) {
            g_warning("NSM: failed to load session: %s", err);
            free(err);
        }
    }
    if (server)
        source_id = g_timeout_add(50, poll_server, server);
}

void nsm_session_manager::disconnect()
{
    if (source_id) {
        g_source_remove(source_id);
        source_id = 0;
    }
    if (server) {
        lo_server_free(server);
        server = NULL;
    }
    if (nsm_addr) {
        lo_address_free(nsm_addr);
        nsm_addr = NULL;
    }
}

int nsm_session_manager::on_reply(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
    nsm_session_manager *self = (nsm_session_manager *)user_data;
    if (argc < 1 || types[0] != 's') return 0;
    if (strcmp(&argv[0]->s, "/nsm/server/announce") != 0) return 0;
    if (argc >= 3 && types[2] == 's')
        self->client_id = &argv[2]->s;
    return 0;
}

int nsm_session_manager::on_open(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
    nsm_session_manager *self = (nsm_session_manager *)user_data;
    std::string new_dir = &argv[0]->s;
    self->client_id = &argv[2]->s;

    g_mkdir_with_parents(new_dir.c_str(), 0755);

    std::string rack_file = new_dir + G_DIR_SEPARATOR_S "rack.xml";
    self->being_restored = g_file_test(rack_file.c_str(), G_FILE_TEST_EXISTS) == TRUE;
    self->session_dir = new_dir;
    self->got_open = true;

    lo_send(self->nsm_addr, "/reply", "ss", "/nsm/client/open", "opened");
    return 0;
}

int nsm_session_manager::on_save(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
    nsm_session_manager *self = (nsm_session_manager *)user_data;
    if (!self->session_dir.empty()) {
        std::string rack_file = self->session_dir + G_DIR_SEPARATOR_S "rack.xml";
        char *err = self->client->save_file(rack_file.c_str());
        if (err) {
            lo_send(self->nsm_addr, "/error", "sis", "/nsm/client/save", 1, err);
            free(err);
            return 0;
        }
    }
    lo_send(self->nsm_addr, "/reply", "ss", "/nsm/client/save", "saved");
    return 0;
}

int nsm_session_manager::on_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
    nsm_session_manager *self = (nsm_session_manager *)user_data;
    self->client->quit();
    return 0;
}

session_manager_iface *calf_plugins::create_nsm_session_mgr(session_client_iface *client, const char *exe_name)
{
    const char *nsm_url = getenv("NSM_URL");
    if (!nsm_url)
        return NULL;
    return new nsm_session_manager(client, nsm_url, exe_name);
}

#endif
