#include "Arena.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace
{
    std::mt19937& rng()
    {
        static std::mt19937 gen(
            static_cast<unsigned int>(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        return gen;
    }

    int rand_range(int lo, int hi)
    {
        std::uniform_int_distribution<int> d(lo, hi);
        return d(rng());
    }

    const char* weapon_name(WeaponType w)
    {
        switch (w)
        {
            case railgun:      return "railgun";
            case flamethrower: return "flamethrower";
            case grenade:      return "grenade launcher";
            case hammer:       return "hammer";
        }
        return "unknown";
    }

    int sign_of(int v)
    {
        return (v > 0) - (v < 0);
    }
}

Arena::Arena() = default;

Arena::~Arena()
{
    for (auto& lr : m_robots)
    {
        if (lr.robot) delete lr.robot;
        if (lr.handle) dlclose(lr.handle);
    }
}

bool Arena::load_config(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
    {
        std::cerr << "Could not open config file: " << path << '\n';
        return false;
    }

    std::string line;
    while (std::getline(f, line))
    {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        auto trim = [](std::string& s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
        };
        trim(key);
        trim(val);

        if (key == "Arena_Size")
        {
            std::istringstream iss(val);
            iss >> m_rows >> m_cols;
        }
        else if (key == "Max_Rounds")        m_max_rounds = std::stoi(val);
        else if (key == "Sleep_interval")    m_sleep_interval = std::stod(val);
        else if (key == "Game_State_Live")   m_live_display = (val == "true" || val == "True" || val == "1");
        else if (key == "Flamethrowers")     m_num_flamethrowers = std::stoi(val);
        else if (key == "Pits")              m_num_pits = std::stoi(val);
        else if (key == "Mounds")            m_num_mounds = std::stoi(val);
    }

    if (m_rows <= 0 || m_cols <= 0)
    {
        std::cerr << "Invalid arena dimensions in config\n";
        return false;
    }

    m_board.assign(m_rows, std::vector<char>(m_cols, '.'));
    return true;
}

bool Arena::load_robots()
{
    const std::string robots_dir = "robots";
    std::vector<std::string> robot_files;

    DIR* d = opendir(robots_dir.c_str());
    if (!d)
    {
        std::cerr << "Could not open robots/ subdirectory.\n";
        return false;
    }
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr)
    {
        std::string name = ent->d_name;
        if (name.size() > 4 &&
            name.compare(0, 6, "Robot_") == 0 &&
            name.substr(name.size() - 4) == ".cpp")
        {
            robot_files.push_back(robots_dir + "/" + name);
        }
    }
    closedir(d);

    if (robot_files.empty())
    {
        std::cerr << "No Robot_*.cpp files found in robots/\n";
        return false;
    }

    std::sort(robot_files.begin(), robot_files.end());

    const std::string display_chars = "@$#&!%*+=?";
    int char_idx = 0;
    constexpr std::size_t kMaxSummary = 50;

    for (const auto& path : robot_files)
    {
        auto slash = path.find_last_of('/');
        std::string filename = (slash == std::string::npos) ? path : path.substr(slash + 1);
        std::string base = filename.substr(0, filename.size() - 4);
        std::string shared_lib = robots_dir + "/lib" + base + ".so";

        std::string compile_cmd =
            "g++ -shared -fPIC -o " + shared_lib + " " + path +
            " RobotBase.o -I. -std=c++20";
        std::cout << "Compiling " << path << " -> " << shared_lib << "\n";
        if (std::system(compile_cmd.c_str()) != 0)
        {
            std::cerr << "Failed to compile " << path << "\n";
            continue;
        }

        void* handle = dlopen(shared_lib.c_str(), RTLD_LAZY);
        if (!handle)
        {
            std::cerr << "dlopen failed for " << shared_lib << ": " << dlerror() << "\n";
            continue;
        }

        RobotFactory factory = (RobotFactory)dlsym(handle, "create_robot");
        if (!factory)
        {
            std::cerr << "create_robot missing in " << shared_lib << "\n";
            dlclose(handle);
            continue;
        }

        using SummaryFn = const char* (*)();
        SummaryFn summary_fn = reinterpret_cast<SummaryFn>(dlsym(handle, "robot_summary"));
        if (!summary_fn)
        {
            std::cerr << "robot_summary missing in " << shared_lib << "\n";
            dlclose(handle);
            continue;
        }
        const char* summary = summary_fn();
        if (!summary)
        {
            std::cerr << "robot_summary returned null in " << shared_lib << "\n";
            dlclose(handle);
            continue;
        }
        std::size_t slen = std::strlen(summary);
        if (slen == 0 || slen > kMaxSummary)
        {
            std::cerr << "robot_summary length " << slen
                      << " out of [1," << kMaxSummary << "] in " << shared_lib << "\n";
            dlclose(handle);
            continue;
        }

        RobotBase* r = factory();
        if (!r)
        {
            std::cerr << "create_robot returned null for " << shared_lib << "\n";
            dlclose(handle);
            continue;
        }

        r->set_boundaries(m_rows, m_cols);
        char ch = display_chars[char_idx % display_chars.size()];
        ++char_idx;
        r->m_character = ch;
        std::string display_name = base;
        if (display_name.compare(0, 6, "Robot_") == 0)
            display_name = display_name.substr(6);
        r->m_name = display_name;

        LoadedRobot lr;
        lr.robot = r;
        lr.handle = handle;
        lr.display_char = ch;
        lr.alive = true;
        lr.source_file = path;
        m_robots.push_back(lr);

        std::cout << "Loaded " << r->m_name << " (" << ch << ") from " << path
                  << " - " << summary << "\n";
    }

    return !m_robots.empty();
}

void Arena::place_obstacles()
{
    auto place = [&](char ch, int count) {
        int placed = 0;
        int attempts = 0;
        const int max_attempts = m_rows * m_cols * 4;
        while (placed < count && attempts < max_attempts)
        {
            int r = rand_range(0, m_rows - 1);
            int c = rand_range(0, m_cols - 1);
            if (m_board[r][c] == '.')
            {
                m_board[r][c] = ch;
                ++placed;
            }
            ++attempts;
        }
    };
    place('M', m_num_mounds);
    place('P', m_num_pits);
    place('F', m_num_flamethrowers);
}

void Arena::place_robots()
{
    for (auto& lr : m_robots)
    {
        int attempts = 0;
        const int max_attempts = m_rows * m_cols * 4;
        while (attempts < max_attempts)
        {
            int r = rand_range(0, m_rows - 1);
            int c = rand_range(0, m_cols - 1);
            if (m_board[r][c] == '.' && find_robot_at(r, c) < 0)
            {
                lr.robot->move_to(r, c);
                break;
            }
            ++attempts;
        }
    }
}

bool Arena::in_bounds(int row, int col) const
{
    return row >= 0 && row < m_rows && col >= 0 && col < m_cols;
}

int Arena::alive_count() const
{
    int n = 0;
    for (const auto& lr : m_robots) if (lr.alive) ++n;
    return n;
}

int Arena::find_robot_at(int row, int col) const
{
    for (size_t i = 0; i < m_robots.size(); ++i)
    {
        int rr, cc;
        m_robots[i].robot->get_current_location(rr, cc);
        if (rr == row && cc == col) return static_cast<int>(i);
    }
    return -1;
}

void Arena::render() const
{
    std::cout << "    ";
    for (int c = 0; c < m_cols; ++c)
    {
        std::cout << std::setw(3) << c;
    }
    std::cout << '\n';

    for (int r = 0; r < m_rows; ++r)
    {
        std::cout << std::setw(3) << r << ' ';
        for (int c = 0; c < m_cols; ++c)
        {
            int idx = find_robot_at(r, c);
            if (idx >= 0)
            {
                if (m_robots[idx].alive)
                    std::cout << "R" << m_robots[idx].display_char << ' ';
                else
                    std::cout << "X" << m_robots[idx].display_char << ' ';
                continue;
            }
            char ch = m_board[r][c];
            if      (ch == '.') std::cout << " . ";
            else if (ch == 'M') std::cout << " M ";
            else if (ch == 'P') std::cout << " P ";
            else if (ch == 'F') std::cout << " F ";
            else                std::cout << " ? ";
        }
        std::cout << '\n';
    }
    std::cout << '\n';
}

void Arena::radar_collect(int row, int col, int self_row, int self_col,
                          std::vector<RadarObj>& out) const
{
    if (!in_bounds(row, col)) return;
    if (row == self_row && col == self_col) return;

    int idx = find_robot_at(row, col);
    if (idx >= 0)
    {
        char type = m_robots[idx].alive ? 'R' : 'X';
        out.emplace_back(type, row, col);
        return;
    }
    char ch = m_board[row][col];
    if (ch == 'M' || ch == 'P' || ch == 'F')
    {
        out.emplace_back(ch, row, col);
    }
}

std::vector<RadarObj> Arena::radar_scan(int robot_index, int direction) const
{
    std::vector<RadarObj> out;
    int row, col;
    m_robots[robot_index].robot->get_current_location(row, col);

    if (direction == 0)
    {
        for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc)
                if (dr || dc) radar_collect(row + dr, col + dc, row, col, out);
        return out;
    }

    if (direction < 1 || direction > 8) return out;

    int dr = directions[direction].first;
    int dc = directions[direction].second;
    int pr = dc;
    int pc = -dr;

    int max_steps = std::max(m_rows, m_cols);
    for (int k = 1; k <= max_steps; ++k)
    {
        int cr = row + dr * k;
        int cc = col + dc * k;
        if (!in_bounds(cr, cc)) break;
        radar_collect(cr - pr, cc - pc, row, col, out);
        radar_collect(cr,      cc,      row, col, out);
        radar_collect(cr + pr, cc + pc, row, col, out);
    }
    return out;
}

int Arena::roll_damage(WeaponType w) const
{
    switch (w)
    {
        case railgun:      return rand_range(10, 20);
        case hammer:       return rand_range(50, 60);
        case grenade:      return rand_range(10, 40);
        case flamethrower: return rand_range(30, 50);
    }
    return 0;
}

void Arena::apply_damage(int target_index, int damage, const std::string& note)
{
    LoadedRobot& tgt = m_robots[target_index];
    if (!tgt.alive) return;

    int armor = tgt.robot->get_armor();
    int reduced = damage - (damage * armor) / 10;
    if (reduced < 0) reduced = 0;
    tgt.robot->take_damage(reduced);
    tgt.robot->reduce_armor(1);

    std::cout << "    " << note << " -> " << tgt.robot->m_name
              << " takes " << reduced << " damage (H="
              << tgt.robot->get_health() << ")\n";

    if (tgt.robot->get_health() <= 0)
    {
        tgt.alive = false;
        std::cout << "    " << tgt.robot->m_name << " is destroyed\n";
    }
}

void Arena::handle_shot(int robot_index, int target_row, int target_col)
{
    LoadedRobot& shooter = m_robots[robot_index];
    int sr, sc;
    shooter.robot->get_current_location(sr, sc);
    WeaponType w = shooter.robot->get_weapon();

    std::cout << "    " << shooter.robot->m_name
              << " fires " << weapon_name(w)
              << " at (" << target_row << "," << target_col << ")\n";

    if (w == railgun)
    {
        int total_dr = target_row - sr;
        int total_dc = target_col - sc;
        if (total_dr == 0 && total_dc == 0) return;

        int abs_dr = std::abs(total_dr);
        int abs_dc = std::abs(total_dc);
        int step_r = sign_of(total_dr);
        int step_c = sign_of(total_dc);

        int r = sr, c = sc, err = 0;
        int max_iters = (m_rows + m_cols) * 2;
        for (int i = 0; i < max_iters; ++i)
        {
            if (abs_dc >= abs_dr)
            {
                c += step_c;
                err += abs_dr;
                if (2 * err >= abs_dc)
                {
                    r += step_r;
                    err -= abs_dc;
                }
            }
            else
            {
                r += step_r;
                err += abs_dc;
                if (2 * err >= abs_dr)
                {
                    c += step_c;
                    err -= abs_dr;
                }
            }
            if (!in_bounds(r, c)) break;

            int idx = find_robot_at(r, c);
            if (idx >= 0 && idx != robot_index && m_robots[idx].alive)
            {
                apply_damage(idx, roll_damage(w), "  railgun");
            }
        }
    }
    else if (w == hammer)
    {
        int rel_r = target_row - sr;
        int rel_c = target_col - sc;
        int chebyshev = std::max(std::abs(rel_r), std::abs(rel_c));
        if (chebyshev != 1)
        {
            std::cout << "    hammer fizzles (target not adjacent)\n";
            return;
        }
        int idx = find_robot_at(target_row, target_col);
        if (idx >= 0 && idx != robot_index && m_robots[idx].alive)
        {
            apply_damage(idx, roll_damage(w), "  hammer");
        }
        else
        {
            std::cout << "    hammer hits empty cell\n";
        }
    }
    else if (w == flamethrower)
    {
        int dr = sign_of(target_row - sr);
        int dc = sign_of(target_col - sc);
        if (dr == 0 && dc == 0) return;
        int pr = dc, pc = -dr;
        for (int k = 1; k <= 4; ++k)
        {
            int cr = sr + dr * k;
            int cc = sc + dc * k;
            for (int off = -1; off <= 1; ++off)
            {
                int rr = cr + pr * off;
                int cc2 = cc + pc * off;
                if (!in_bounds(rr, cc2)) continue;
                int idx = find_robot_at(rr, cc2);
                if (idx >= 0 && m_robots[idx].alive)
                {
                    apply_damage(idx, roll_damage(w), "  flame");
                }
            }
        }
    }
    else if (w == grenade)
    {
        if (shooter.robot->get_grenades() <= 0)
        {
            std::cout << "    out of grenades\n";
            return;
        }
        shooter.robot->decrement_grenades();
        for (int dr = -1; dr <= 1; ++dr)
        {
            for (int dc = -1; dc <= 1; ++dc)
            {
                int rr = target_row + dr;
                int cc = target_col + dc;
                if (!in_bounds(rr, cc)) continue;
                int idx = find_robot_at(rr, cc);
                if (idx >= 0 && m_robots[idx].alive)
                {
                    apply_damage(idx, roll_damage(w), "  grenade");
                }
            }
        }
    }
}

void Arena::handle_move(int robot_index, int direction, int distance)
{
    LoadedRobot& lr = m_robots[robot_index];
    if (!lr.alive) return;
    if (direction < 1 || direction > 8 || distance <= 0)
    {
        std::cout << "    " << lr.robot->m_name << " holds position\n";
        return;
    }

    int max_speed = lr.robot->get_move_speed();
    if (max_speed <= 0)
    {
        std::cout << "    " << lr.robot->m_name << " is immobilized\n";
        return;
    }
    int steps = std::min(distance, max_speed);

    int cr, cc;
    lr.robot->get_current_location(cr, cc);
    int dr = directions[direction].first;
    int dc = directions[direction].second;

    int prev_r = cr, prev_c = cc;
    bool stopped_in_pit = false;

    for (int s = 0; s < steps; ++s)
    {
        int nr = prev_r + dr;
        int nc = prev_c + dc;
        if (!in_bounds(nr, nc)) break;

        if (find_robot_at(nr, nc) >= 0) break;

        char terrain = m_board[nr][nc];
        if (terrain == 'M') break;

        if (terrain == 'F')
        {
            int dmg = roll_damage(flamethrower);
            int armor = lr.robot->get_armor();
            int reduced = dmg - (dmg * armor) / 10;
            if (reduced < 0) reduced = 0;
            lr.robot->take_damage(reduced);
            lr.robot->reduce_armor(1);
            std::cout << "    " << lr.robot->m_name
                      << " burns crossing flame at (" << nr << "," << nc
                      << ") for " << reduced << " (H="
                      << lr.robot->get_health() << ")\n";
            prev_r = nr; prev_c = nc;
            if (lr.robot->get_health() <= 0)
            {
                lr.alive = false;
                std::cout << "    " << lr.robot->m_name
                          << " dies on flame; flame consumed\n";
                m_board[nr][nc] = '.';
                break;
            }
            continue;
        }

        if (terrain == 'P')
        {
            prev_r = nr; prev_c = nc;
            lr.robot->disable_movement();
            std::cout << "    " << lr.robot->m_name
                      << " falls into pit at (" << nr << "," << nc << ")\n";
            stopped_in_pit = true;
            break;
        }

        prev_r = nr; prev_c = nc;
    }

    (void)stopped_in_pit;

    if (prev_r == cr && prev_c == cc)
    {
        std::cout << "    " << lr.robot->m_name << " cannot move\n";
        return;
    }

    lr.robot->move_to(prev_r, prev_c);

    std::cout << "    " << lr.robot->m_name
              << " moves to (" << prev_r << "," << prev_c << ")\n";
}

void Arena::log(const std::string& s) const
{
    std::cout << s << '\n';
}

void Arena::run()
{
    m_current_round = 1;

    while (m_current_round <= m_max_rounds)
    {
        bool ended = false;
        for (size_t i = 0; i < m_robots.size(); ++i)
        {
            std::cout << "\n========== round " << m_current_round << " ==========\n";
            render();

            if (alive_count() <= 1) { ended = true; break; }

            LoadedRobot& lr = m_robots[i];
            if (!lr.alive) continue;

            std::cout << lr.robot->print_stats() << "\n";

            int radar_dir = 0;
            lr.robot->get_radar_direction(radar_dir);
            std::vector<RadarObj> hits = radar_scan(static_cast<int>(i), radar_dir);
            std::cout << "    radar dir=" << radar_dir << " returned ";
            if (hits.empty())
            {
                std::cout << "nothing";
            }
            else
            {
                for (size_t h = 0; h < hits.size(); ++h)
                {
                    if (h > 0) std::cout << ", ";
                    std::cout << hits[h].m_type
                              << " at " << hits[h].m_row << "," << hits[h].m_col;
                }
            }
            std::cout << "\n";
            lr.robot->process_radar_results(hits);

            int shot_r = 0, shot_c = 0;
            bool shoots = lr.robot->get_shot_location(shot_r, shot_c);
            if (shoots)
            {
                if (in_bounds(shot_r, shot_c))
                {
                    handle_shot(static_cast<int>(i), shot_r, shot_c);
                }
                else
                {
                    std::cout << "    shot target out of bounds, skipping\n";
                }
            }
            else
            {
                int mdir = 0, mdist = 0;
                lr.robot->get_move_direction(mdir, mdist);
                handle_move(static_cast<int>(i), mdir, mdist);
            }

            if (m_live_display && m_sleep_interval > 0)
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(m_sleep_interval * 1000)));
            }
        }
        if (ended) break;
        ++m_current_round;
    }

    std::cout << "\n========== game over ==========\n";
    render();
    int alive = alive_count();
    if (alive == 1)
    {
        for (const auto& lr : m_robots)
        {
            if (lr.alive)
            {
                std::cout << "Winner: " << lr.robot->m_name
                          << " (" << lr.display_char << ")\n";
                break;
            }
        }
    }
    else if (alive == 0)
    {
        std::cout << "No survivors.\n";
    }
    else
    {
        std::cout << "Round limit reached. " << alive << " robots still alive.\n";
        for (const auto& lr : m_robots)
        {
            if (lr.alive) std::cout << "  " << lr.robot->print_stats() << "\n";
        }
    }
}
