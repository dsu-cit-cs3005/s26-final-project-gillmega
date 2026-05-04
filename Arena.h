#pragma once

#include "RobotBase.h"
#include "RadarObj.h"

#include <string>
#include <vector>

struct LoadedRobot
{
    RobotBase* robot = nullptr;
    void* handle = nullptr;
    char display_char = '?';
    bool alive = true;
    std::string source_file;
};

class Arena
{
public:
    Arena();
    ~Arena();

    bool load_config(const std::string& path);
    bool load_robots();
    void place_obstacles();
    void place_robots();
    void run();

private:
    int m_rows = 0;
    int m_cols = 0;
    int m_max_rounds = 0;
    double m_sleep_interval = 0.0;
    bool m_live_display = false;
    int m_num_flamethrowers = 0;
    int m_num_pits = 0;
    int m_num_mounds = 0;

    std::vector<std::vector<char>> m_board;
    std::vector<LoadedRobot> m_robots;
    int m_current_round = 0;

    void render() const;
    int alive_count() const;
    int find_robot_at(int row, int col) const;
    bool in_bounds(int row, int col) const;

    std::vector<RadarObj> radar_scan(int robot_index, int direction) const;
    void radar_collect(int row, int col, int self_row, int self_col,
                       std::vector<RadarObj>& out) const;

    void handle_shot(int robot_index, int target_row, int target_col);
    void apply_damage(int target_index, int damage, const std::string& note);
    int roll_damage(WeaponType w) const;

    void handle_move(int robot_index, int direction, int distance);

    void log(const std::string& s) const;
};
