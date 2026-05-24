#include <iostream>
#include <ncurses.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>

#define PORT 8080
#define IP   "127.0.0.1"

int g_sock;

void net_send(const std::string& line) {
    std::string s = line + "\n";
    send(g_sock, s.c_str(), s.size(), 0);
}

std::string net_recv_string() {
    size_t len = 0;
    recv(g_sock, &len, sizeof(len), MSG_WAITALL);
    std::string s(len, '\0');
    recv(g_sock, &s[0], len, MSG_WAITALL);
    return s;
}

void print_center(int y, const std::string& s) {
    int x = (COLS - (int)s.size()) / 2;
    if (x < 0) x = 0;
    mvprintw(y, x, "%s", s.c_str());
}

std::string read_line_input(int y, int x, bool secret = false) {
    echo(); curs_set(1);
    if (secret) noecho();
    char buf[256] = {};
    move(y, x);
    getnstr(buf, 255);
    noecho(); curs_set(0);
    return buf;
}

void status_bar(const std::string& msg, int color) {
    attron(COLOR_PAIR(color) | A_BOLD);
    mvprintw(LINES - 2, 2, "%-*s", COLS - 4, msg.c_str());
    attroff(COLOR_PAIR(color) | A_BOLD);
    refresh();
}

void draw_box_title(const std::string& title) {
    clear();
    box(stdscr, 0, 0);
    attron(A_BOLD | COLOR_PAIR(2));
    print_center(1, title);
    attroff(A_BOLD | COLOR_PAIR(2));
}

int scrollable_list(const std::string& title,
                    const std::vector<std::string>& items,
                    const std::string& hint = "Enter=select  q=back") {
    if (items.empty()) return -1;

    int selected = 0;
    int scroll   = 0;
    int max_visible = LINES - 7;

    while (true) {
        draw_box_title(title);
        mvprintw(LINES - 2, 2, "%s", hint.c_str());

        for (int i = 0; i < max_visible && (scroll + i) < (int)items.size(); i++) {
            int idx = scroll + i;
            if (idx == selected) attron(COLOR_PAIR(1) | A_BOLD);
            mvprintw(3 + i, 4, "%2d. %-*s", idx + 1, COLS - 10, items[idx].c_str());
            if (idx == selected) attroff(COLOR_PAIR(1) | A_BOLD);
        }

        if ((int)items.size() > max_visible) {
            mvprintw(LINES - 3, COLS - 14, " %d/%d ", selected + 1, (int)items.size());
        }

        refresh();

        int ch = getch();
        if (ch == KEY_UP   || ch == 'k') {
            if (selected > 0) {
                selected--;
                if (selected < scroll) scroll = selected;
            }
        } else if (ch == KEY_DOWN || ch == 'j') {
            if (selected < (int)items.size() - 1) {
                selected++;
                if (selected >= scroll + max_visible) scroll = selected - max_visible + 1;
            }
        } else if (ch == '\n' || ch == KEY_ENTER) {
            return selected;
        } else if (ch == 'q' || ch == 'Q') {
            return -1;
        }
    }
}

std::string screen_auth() {
    while (true) {
        draw_box_title("=== MUSIC CLIENT ===");
        print_center(4, "[1] Login");
        print_center(5, "[2] Sign Up");
        print_center(6, "[q] Quit");
        refresh();

        int ch = getch();
        if (ch == 'q' || ch == 'Q') return "\x01";
        if (ch != '1' && ch != '2') continue;
        bool is_login = (ch == '1');

        draw_box_title(is_login ? "-- LOGIN --" : "-- SIGN UP --");
        mvprintw(5, 4, "Username: ");
        std::string user = read_line_input(5, 14);

        mvprintw(7, 4, "Password: ");
        std::string pass = read_line_input(7, 14, true);

        status_bar("Connecting...", 3);
        net_send((is_login ? "login " : "signup ") + user + " " + pass);
        std::string res = net_recv_string();

        if (res == "LOGIN_OK") {
            status_bar("Welcome, " + user + "!", 1);
            napms(700);
            return user;
        }
        if (res == "SIGNUP_OK") {
            status_bar("Account created! Now log in.", 1);
            napms(1000);
            continue;
        }
        status_bar(res.empty() ? "Server error" : res, 4);
        napms(1200);
    }
}

bool download_song(const std::string& song, const std::string& path) {
    net_send("play " + song);

    size_t file_size = 0;
    recv(g_sock, &file_size, sizeof(file_size), MSG_WAITALL);
    if (file_size == 0) return false;

    std::ofstream f(path, std::ios::binary);
    char buf[4096];
    size_t got = 0;

    while (got < file_size) {
        int n = recv(g_sock, buf, std::min(sizeof(buf), file_size - got), 0);
        if (n <= 0) break;
        f.write(buf, n);
        got += n;

        int bar = (int)(got * (size_t)(COLS - 12) / file_size);
        mvprintw(LINES / 2 + 2, 4, "[%-*s] %3d%%",
                 COLS - 12, std::string(bar, '#').c_str(),
                 (int)(got * 100 / file_size));
        refresh();
    }
    return got >= file_size;
}

struct YtResult {
    std::string id;
    std::string title;
    std::string duration;
};

static std::string json_field(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\": \"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        // Попробуем числовой вариант для duration_string
        needle = "\"" + key + "\": ";
        pos = json.find(needle);
        if (pos == std::string::npos) return "";
        size_t start = pos + needle.size();
        if (json[start] == '"') { start++; }
        size_t end = json.find_first_of(",}\n", start);
        std::string val = json.substr(start, end - start);
        // убираем кавычки
        if (!val.empty() && val.front() == '"') val = val.substr(1);
        if (!val.empty() && val.back()  == '"') val.pop_back();
        return val;
    }
    size_t start = pos + needle.size();
    size_t end   = json.find('"', start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
}

std::vector<YtResult> yt_search(const std::string& query) {
    std::vector<YtResult> results;

    std::string tmpfile = "/tmp/yt_search_out.json";
    std::string cmd = "yt-dlp --dump-json --flat-playlist --no-warnings "
                      "\"ytsearch5:" + query + "\" > " + tmpfile + " 2>/dev/null";
    int ret = system(cmd.c_str());
    (void)ret;

    std::ifstream f(tmpfile);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        YtResult r;
        r.id       = json_field(line, "id");
        r.title    = json_field(line, "title");
        r.duration = json_field(line, "duration_string");
        if (!r.id.empty() && !r.title.empty())
            results.push_back(r);
    }
    return results;
}

bool yt_download_audio(const std::string& video_id, const std::string& out_path) {
    // Удаляем старый файл
    remove(out_path.c_str());

    std::string url = "https://www.youtube.com/watch?v=" + video_id;
    std::string tpl = "/tmp/yt_stream.%(ext)s";

    std::string cmd = "yt-dlp -x --audio-format mp3 --audio-quality 0 "
                      "--no-playlist --no-warnings "
                      "-o \"" + tpl + "\" \"" + url + "\" >/dev/null 2>&1";
    int ret = system(cmd.c_str());
    return ret == 0 && access(out_path.c_str(), F_OK) == 0;
}

void screen_youtube() {
    while (true) {
        draw_box_title("=== YOUTUBE SEARCH ===");
        mvprintw(4, 4, "Query (q=back): ");
        refresh();

        echo(); curs_set(1);
        char buf[256] = {};
        move(4, 20);
        getnstr(buf, 255);
        noecho(); curs_set(0);

        std::string query = buf;
        if (query == "q" || query == "Q" || query.empty()) return;

        // Поиск
        draw_box_title("=== YOUTUBE SEARCH ===");
        status_bar("Searching... (yt-dlp)", 3);
        refresh();

        std::vector<YtResult> results = yt_search(query);

        if (results.empty()) {
            status_bar("No results (is yt-dlp installed? pip install yt-dlp)", 4);
            napms(2000);
            continue;
        }

        std::vector<std::string> titles;
        for (auto& r : results) {
            std::string t = r.title;
            if (!r.duration.empty()) t += "  [" + r.duration + "]";
            titles.push_back(t);
        }

        int idx = scrollable_list("=== SELECT TRACK ===", titles,
                                  "Enter=play  q=back");
        if (idx < 0) continue;

        std::string out_path = "/tmp/yt_stream.mp3";
        draw_box_title(results[idx].title);
        print_center(LINES / 2, "Downloading audio via yt-dlp...");
        print_center(LINES / 2 + 1, "(ffmpeg required for mp3 conversion)");
        refresh();

        bool ok = yt_download_audio(results[idx].id, out_path);

        if (!ok) {
            status_bar("Download failed. Check: yt-dlp, ffmpeg", 4);
            napms(2000);
            continue;
        }

        endwin();
        std::cout << "\nPlaying: " << results[idx].title << "\n";
        std::cout << "(Press Ctrl+C to stop)\n";
        system(("mpg123 \"" + out_path + "\"").c_str());
        initscr(); cbreak(); noecho(); curs_set(0);
        keypad(stdscr, TRUE);
        status_bar("Done: " + results[idx].title, 1);
        napms(1000);
    }
}

void screen_main(const std::string& username) {
    std::vector<std::string> playlist;

    while (true) {
        draw_box_title("=== MUSIC CLIENT ===");
        attron(COLOR_PAIR(1));
        print_center(2, "User: " + username);
        attroff(COLOR_PAIR(1));

        mvprintw(5, 4, "[1] List songs");
        mvprintw(6, 4, "[2] Play  (stream to /tmp/)");
        mvprintw(7, 4, "[3] Download (save to ~/Downloads/)");
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(8, 4, "[4] YouTube search");
        attroff(COLOR_PAIR(4) | A_BOLD);
        mvprintw(9, 4, "[5] Logout");
        mvprintw(10, 4, "[q] Quit");

        if (!playlist.empty()) {
            attron(COLOR_PAIR(3));
            mvprintw(12, 4, "Playlist: %d tracks cached", (int)playlist.size());
            attroff(COLOR_PAIR(3));
        }
        refresh();

        int ch = getch();

        if (ch == '1') {
            net_send("list");
            std::string raw = net_recv_string();
            playlist.clear();
            std::istringstream ss(raw);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) playlist.push_back(line);
            }
            std::sort(playlist.begin(), playlist.end());

            int idx = scrollable_list("=== PLAYLIST ===", playlist,
                                      "Enter=select  q=back");
            (void)idx;
        }

        else if (ch == '2' || ch == '3') {
            if (playlist.empty()) {
                status_bar("Run [1] List first!", 4);
                napms(1000);
                continue;
            }

            int idx = scrollable_list("=== SELECT TRACK ===", playlist,
                                      "Enter=play  q=back");
            if (idx < 0) continue;

            std::string song = playlist[idx];

            std::string out_path;
            if (ch == '2') {
                out_path = "/tmp/" + song;
            } else {
                const char* home = getenv("HOME");
                out_path = std::string(home ? home : "/tmp") + "/Downloads/" + song;
            }

            draw_box_title(song);
            print_center(LINES / 2, "Downloading...");
            refresh();

            bool ok = download_song(song, out_path);

            if (ok && ch == '2') {
                endwin();
                std::cout << "\nPlaying: " << song << "\n";
                system(("mpg123 \"" + out_path + "\"").c_str());
                initscr(); cbreak(); noecho(); curs_set(0);
                keypad(stdscr, TRUE);
            }
            status_bar(ok ? "Done: " + out_path : "Error: file not found", ok ? 1 : 4);
            napms(1500);
        }

        else if (ch == '4') {
            screen_youtube();
        }

        else if (ch == '5') { return; }
        else if (ch == 'q' || ch == 'Q') { endwin(); exit(0); }
    }
}

int main() {
    g_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(IP);

    if (connect(g_sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Cannot connect to %s:%d\n", IP, PORT);
        return 1;
    }

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_GREEN,  -1);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_CYAN,   -1);
    init_pair(4, COLOR_RED,    -1);

    while (true) {
        std::string user = screen_auth();
        if (user == "\x01") break;
        if (user.empty())   continue;
        screen_main(user);
    }

    endwin();
    close(g_sock);
    return 0;
}