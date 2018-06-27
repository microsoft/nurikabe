// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// Nurikabe Solver by Stephan T. Lavavej
// https://en.wikipedia.org/wiki/Nurikabe_(puzzle)

// cl /EHsc /nologo /W4 /MT /O2 /GL nurikabe.cpp && nurikabe && wikipedia_hard.html

#include <stddef.h>
#include <stdlib.h>
#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <windows.h>
using namespace std;

class Grid {
public:
    Grid(int width, int height, const string& s);

    enum SitRep {
        CONTRADICTION_FOUND,
        SOLUTION_FOUND,
        KEEP_GOING,
        CANNOT_PROCEED
    };

    SitRep solve(bool verbose = true, bool guessing = true);

    int known() const;

    void write(ostream& os, long long start, long long finish) const;

private:
    // The states that a cell can be in. Numbered cells are positive,
    // which is why this has an explicitly specified underlying type.
    enum State : int {
        UNKNOWN = -3,
        WHITE = -2,
        BLACK = -1
    };

    // Each region is black, white, or numbered. This allows us to
    // remember when white cells are connected to numbered cells,
    // as the whole region is marked as numbered.
    // Each region keeps track of the coordinates that it occupies.
    // Each region also keeps track of the unknown cells that it's surrounded by.
    class Region {
    public:
        Region(const State state, const int x, const int y, const set<pair<int, int>>& unknowns)
            : m_state(state), m_coords(), m_unknowns(unknowns) {

            if (state == UNKNOWN) {
                throw logic_error("LOGIC ERROR: Grid::Region::Region() - state must be known!");
            }

            m_coords.insert(make_pair(x, y));
        }


        bool white() const {
            return m_state == WHITE;
        }

        bool black() const {
            return m_state == BLACK;
        }

        bool numbered() const {
            return m_state > 0;
        }

        int number() const {
            if (!numbered()) {
                throw logic_error(
                    "LOGIC ERROR: Grid::Region::number() - This region is not numbered!");
            }

            return m_state;
        }


        set<pair<int, int>>::const_iterator begin() const {
            return m_coords.begin();
        }

        set<pair<int, int>>::const_iterator end() const {
            return m_coords.end();
        }

        int size() const {
            return m_coords.size();
        }

        bool contains(const int x, const int y) const {
            return m_coords.find(make_pair(x, y)) != m_coords.end();
        }

        template <typename InIt> void insert(InIt first, InIt last) {
            m_coords.insert(first, last);
        }


        set<pair<int, int>>::const_iterator unk_begin() const {
            return m_unknowns.begin();
        }

        set<pair<int, int>>::const_iterator unk_end() const {
            return m_unknowns.end();
        }

        int unk_size() const {
            return m_unknowns.size();
        }

        template <typename InIt> void unk_insert(InIt first, InIt last) {
            m_unknowns.insert(first, last);
        }

        void unk_erase(const int x, const int y) {
            m_unknowns.erase(make_pair(x, y));
        }

    private:
        State m_state;
        set<pair<int, int>> m_coords;
        set<pair<int, int>> m_unknowns;
    };

    // We use an upper-left origin.
    // This is convenient during construction and printing.
    // It's irrelevant during analysis.

    bool valid(int x, int y) const;

    State& cell(int x, int y);
    const State& cell(int x, int y) const;

    shared_ptr<Region>& region(int x, int y);
    const shared_ptr<Region>& region(int x, int y) const;

    void print(const string& s, const set<pair<int, int>>& updated = set<pair<int, int>>());
    bool process(bool verbose, const set<pair<int, int>>& mark_as_black,
        const set<pair<int, int>>& mark_as_white, const string& s);

    template <typename F> void for_valid_neighbors(int x, int y, F f) const;
    void insert_valid_neighbors(set<pair<int, int>>& s, int x, int y) const;
    void insert_valid_unknown_neighbors(set<pair<int, int>>& s, int x, int y) const;

    void add_region(int x, int y);
    void mark(State s, int x, int y);
    void fuse_regions(shared_ptr<Region> r1, shared_ptr<Region> r2);

    bool impossibly_big_white_region(int n) const;

    bool unreachable(int x_root, int y_root,
        set<pair<int, int>> discovered = set<pair<int, int>>()) const;

    bool confined(const shared_ptr<Region>& r,
        map<shared_ptr<Region>, set<pair<int, int>>>& cache,
        const set<pair<int, int>>& verboten = set<pair<int, int>>()) const;

    string detect_contradictions(map<shared_ptr<Region>, set<pair<int, int>>>& cache) const;


    int m_width; // x is valid within [0, m_width).
    int m_height; // y is valid within [0, m_height).
    int m_total_black; // The total number of black cells that will be in the solution.

    // m_cells[x][y].first is the state of a cell.
    // m_cells[x][y].second is the region of a cell.
    // (If the state is unknown, the region is empty.)
    vector<vector<pair<State, shared_ptr<Region>>>> m_cells;

    // The set of all regions can be traversed in linear time.
    set<shared_ptr<Region>> m_regions;

    // This is initially KEEP_GOING.
    // If an attempt is made to fuse two numbered regions, or to mark an already known cell,
    // this is set to CONTRADICTION_FOUND.
    SitRep m_sitrep;

    // This stores the output that is generated during solving, to be converted into HTML later.
    vector<tuple<string, vector<vector<State>>, set<pair<int, int>>, long long>> m_output;

    // This is used to guess cells in a deterministic but pseudorandomized order.
    mt19937 m_prng;

    Grid(const Grid& other);
    Grid& operator=(const Grid& other);
    void swap(Grid& other);
};

long long counter() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

long long frequency() {
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    return li.QuadPart;
}

string format_time(const long long start, const long long finish) {
    ostringstream oss;

    if ((finish - start) * 1000 < frequency()) {
        oss << (finish - start) * 1000000.0 / frequency() << " microseconds";
    } else if (finish - start < frequency()) {
        oss << (finish - start) * 1000.0 / frequency() << " milliseconds";
    } else {
        oss << (finish - start) * 1.0 / frequency() << " seconds";
    }

    return oss.str();
}

int main() {
    struct Data {
        const char * name;
        int w;
        int h;
        const char * s;
    };

    const Data data[] = {
        {
            "wikipedia_hard", 10, 9,
            "2        2\n"
            "      2   \n"
            " 2  7     \n"
            "          \n"
            "      3 3 \n"
            "  2    3  \n"
            "2  4      \n"
            "          \n"
            " 1    2 4 \n"
        },

        {
            "wikipedia_easy", 10, 10,
            "1   4  4 2\n"
            "          \n"
            " 1   2    \n"
            "  1   1  2\n"
            "1    3    \n"
            "  6      5\n"
            "          \n"
            "     1   2\n"
            "    2  2  \n"
            "          \n"
        },

        { // http://nikoli.com/en/puzzles/nurikabe/
            "nikoli_1", 10, 10,
            "       5 2\n"
            "3         \n"
            " 4  2     \n"
            "      3   \n"
            " 4   4    \n"
            "         3\n"
            "          \n"
            "          \n"
            " 3  3     \n"
            "  1  1 3 3\n"
        },

        {
            "nikoli_2", 10, 10,
            "6 2 3    3\n"
            "          \n"
            "         4\n"
            "          \n"
            "    2    2\n"
            "3    5    \n"
            "          \n"
            "3         \n"
            "          \n"
            "4    5 4 1\n"
        },

        {
            "nikoli_3", 10, 10,
            " 3    4   \n"
            "     6    \n"
            "       2  \n"
            "      3   \n"
            "        2 \n"
            " 4     3  \n"
            "         1\n"
            " 10      3 \n"
            "          \n"
            "  3      2\n"
        },

        {
            "nikoli_4", 18, 10,
            "  4            1 3\n"
            " 3    5   1 2     \n"
            "       5 3        \n"
            "            2 3   \n"
            "  4             3 \n"
            " 3             4  \n"
            "   1 1            \n"
            "        3 4       \n"
            "     1 1   5    5 \n"
            "4 4            3  \n"
        },

        {
            "nikoli_5", 18, 10,
            " 1 1    1     1   \n"
            "    5    2     1  \n"
            "        1     1   \n"
            "     5         1  \n"
            "1 1       4   1   \n"
            " 1     3     7    \n"
            "  3              6\n"
            "    4   2  4      \n"
            "      5         5 \n"
            " 1           5    \n"
        },

        {
            "nikoli_6", 18, 10,
            "                  \n"
            "1    12     3 12    \n"
            "                 2\n"
            "2    3     3    3 \n"
            "    1     1       \n"
            "3    1            \n"
            "   2  2 3 2       \n"
            "2           1     \n"
            "  3               \n"
            "1              12 1\n"
        },

        {
            "nikoli_7", 24, 14,
            "    5                   \n"
            "          2 6    7 3   4\n"
            "  1    5        3 5     \n"
            " 7   6                 1\n"
            "        4               \n"
            "   1      1   5      3  \n"
            "  2  3                  \n"
            "        3   3   2  7    \n"
            "                        \n"
            "6   1    5   5   1    5 \n"
            "      6        5     3  \n"
            "   4               4    \n"
            " 5          1           \n"
            "        3 4     5       \n"
        },

        {
            "nikoli_8", 24, 14,
            "    2 1           5 5   \n"
            "  4             12     1 \n"
            " 7      1               \n"
            "              1        3\n"
            "          7             \n"
            "6            5          \n"
            "           6           1\n"
            "9           15           \n"
            "          3            3\n"
            "             8          \n"
            "2        8              \n"
            "               4      3 \n"
            " 4     5             3  \n"
            "   8 3           2 4    \n"
        },

        {
            "nikoli_9", 36, 20,
            "2   2  1  1               1         \n"
            "   4    3        9      8      5    \n"
            "      1        7                   5\n"
            "4      1  1  4              2    1  \n"
            "      2  3         2         1 3    \n"
            "4   2           5    2              \n"
            "       1  1 17          3 4        4 \n"
            "                 9              21  2\n"
            "2       2                 4         \n"
            "  7  4            3   13             \n"
            "          1               6    1    \n"
            "  4      2    9  1                  \n"
            "     6               3          9   \n"
            "22                  1      8  1      \n"
            "   1   6   1   4                    \n"
            "    2     2     1      1       1   1\n"
            "                  4     2           \n"
            "   3 3   2   2       8      2     3 \n"
            "            1              1        \n"
            "                3       5       5   \n"
        },

        {
            "nikoli_10", 36, 20,
            "           4            2           \n"
            "3 4          2   7         8      2 \n"
            "    7      5   1   8 5   1  2  4   2\n"
            "6    4       3          2 2         \n"
            "           6                   4    \n"
            "    2             1  2           2  \n"
            "        1       4     4    4  1     \n"
            " 1                  3            4 4\n"
            "     2     4  4            4        \n"
            "       5  3                   2 4   \n"
            " 5 1              1    3   8   2    \n"
            "     1   2                          \n"
            "2            2 5           4     2 1\n"
            "                             2      \n"
            "1  2   4  7   18   1            1   1\n"
            "                     2   8 4        \n"
            "    3           18     1          4  \n"
            "                 4                4 \n"
            "      3 1   4      4    2    4   4  \n"
            "6      1  3                 4       \n"
        },
    };

    try {
        for (auto i = data; i != data + sizeof(data) / sizeof(data[0]); ++i) {
            const long long start = counter();

            Grid g(i->w, i->h, i->s);

            while (g.solve() == Grid::KEEP_GOING) { }

            const long long finish = counter();


            ofstream f(i->name + string(".html"));

            g.write(f, start, finish);


            cout << i->name << ": " << format_time(start, finish) << ", ";

            const int k = g.known();
            const int cells = i->w * i->h;

            cout << k << "/" << cells << " (" << k * 100.0 / cells << "%) solved" << endl;
        }
    } catch (const exception& e) {
        cerr << "EXCEPTION CAUGHT! \"" << e.what() << "\"" << endl;
        return EXIT_FAILURE;
    } catch (...) {
        cerr << "UNKNOWN EXCEPTION CAUGHT!" << endl;
        return EXIT_FAILURE;
    }
}

Grid::Grid(const int width, const int height, const string& s)
    : m_width(width), m_height(height), m_total_black(width * height),
    m_cells(), m_regions(), m_sitrep(KEEP_GOING), m_output(), m_prng(1729) {

    // Validate width and height.

    if (width < 1) {
        throw runtime_error("RUNTIME ERROR: Grid::Grid() - width must be at least 1.");
    }

    if (height < 1) {
        throw runtime_error("RUNTIME ERROR: Grid::Grid() - height must be at least 1.");
    }

    // Initialize m_cells. We must set everything to UNKNOWN before calling add_region() below.

    m_cells.resize(width, vector<pair<State, shared_ptr<Region>>>(
        height, make_pair(UNKNOWN, shared_ptr<Region>())));

    // Parse the string.

    vector<int> v;

    const regex r("(\\d+)|( )|(\\n)|[^\\d \\n]");

    for (sregex_iterator i(s.begin(), s.end(), r), end; i != end; ++i) {
        const smatch& m = *i;

        if (m[1].matched) {
            v.push_back(stoi(m[1]));
        } else if (m[2].matched) {
            v.push_back(0);
        } else if (m[3].matched) {
            // Do nothing.
        } else {
            throw runtime_error("RUNTIME ERROR: Grid::Grid() - "
                "s must contain only digits, spaces, and newlines.");
        }
    }

    // Validate the number of cells. Note that we can't do this before
    // parsing, because numbers 10 and above occupy multiple characters.

    if (v.size() != static_cast<size_t>(width * height)) {
        throw runtime_error("RUNTIME ERROR: Grid::Grid() - "
            "s must contain width * height numbers and spaces.");
    }

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            const int n = v[x + y * width];

            if (n > 0) {
                // Testing x - 1 and x + 1 is unnecessary because
                // horizontally adjacent numbers would have been concatenated.
                // Testing y + 1 is unnecessary because if there are
                // vertically adjacent numbers, one's above and one's below.

                if (valid(x, y - 1) && cell(x, y - 1) > 0) {
                    throw runtime_error("RUNTIME ERROR: Grid::Grid() - "
                        "s contains vertically adjacent numbers.");
                }

                // Set the cell's state and region.

                cell(x, y) = static_cast<State>(n);

                add_region(x, y);

                // m_total_black is width * height - (sum of all numbered cells).
                // Calculating this allows us to determine whether black regions
                // are partial or complete with a simple size test.
                // Humans are capable of this but it's not convenient for them.

                m_total_black -= n;
            }
        }
    }

    print("I'm okay to go!");
}

Grid::SitRep Grid::solve(const bool verbose, const bool guessing) {
    map<shared_ptr<Region>, set<pair<int, int>>> cache;


    // Look for contradictions. Do this first.

    {
        const string s = detect_contradictions(cache);

        if (!s.empty()) {
            if (verbose) {
                print(s);
            }

            return CONTRADICTION_FOUND;
        }
    }


    // See if we're done. Do this second.

    if (known() == m_width * m_height) {
        if (verbose) {
            print("I'm done!");
        }

        return SOLUTION_FOUND;
    }


    set<pair<int, int>> mark_as_black;
    set<pair<int, int>> mark_as_white;


    // Look for complete islands.

    for (auto i = m_regions.begin(); i != m_regions.end(); ++i) {
        const Region& r = **i;

        if (r.numbered() && r.size() == r.number()) {
            mark_as_black.insert(r.unk_begin(), r.unk_end());
        }
    }

    if (process(verbose, mark_as_black, mark_as_white, "Complete islands found.")) {
        return m_sitrep;
    }


    // Look for partial regions that can expand into only one cell. They must expand.

    for (auto i = m_regions.begin(); i != m_regions.end(); ++i) {
        const Region& r = **i;

        const bool partial =
            r.black() && r.size() < m_total_black
                || r.white()
                || r.numbered() && r.size() < r.number();

        if (partial && r.unk_size() == 1) {
            if (r.black()) {
                mark_as_black.insert(*r.unk_begin());
            } else {
                mark_as_white.insert(*r.unk_begin());
            }
        }
    }

    if (process(verbose, mark_as_black, mark_as_white,
        "Expanded partial regions with only one liberty.")) {
        return m_sitrep;
    }


    // Look for N - 1 islands with exactly two diagonal liberties.

    for (auto i = m_regions.begin(); i != m_regions.end(); ++i) {
        const Region& r = **i;

        if (r.numbered() && r.size() == r.number() - 1 && r.unk_size() == 2) {
            const int x1 = r.unk_begin()->first;
            const int y1 = r.unk_begin()->second;
            const int x2 = next(r.unk_begin())->first;
            const int y2 = next(r.unk_begin())->second;

            if (abs(x1 - x2) == 1 && abs(y1 - y2) == 1) {
                pair<int, int> p;

                if (r.contains(x1, y2)) {
                    p = make_pair(x2, y1);
                } else {
                    p = make_pair(x1, y2);
                }

                // The far cell might already be black, in which case there's nothing to do.
                // It could even be white/numbered (if it's part of this island), in which case
                // there's still nothing to do.
                // (If it's white/numbered and not part of this island,
                // we'll eventually detect a contradiction.)

                if (cell(p.first, p.second) == UNKNOWN) {
                    mark_as_black.insert(p);
                }
            }
        }
    }

    if (process(verbose, mark_as_black, mark_as_white,
        "N - 1 islands with exactly two diagonal liberties found.")) {
        return m_sitrep;
    }


    // Look for unreachable cells. They must be black.
    // This supersedes complete island analysis and forbidden bridge analysis.
    // (We run complete island analysis above because it's fast
    // and it makes the output easier to understand.)

    for (int x = 0; x < m_width; ++x) {
        for (int y = 0; y < m_height; ++y) {
            if (unreachable(x, y)) {
                mark_as_black.insert(make_pair(x, y));
            }
        }
    }

    if (process(verbose, mark_as_black, mark_as_white, "Unreachable cells blackened.")) {
        return m_sitrep;
    }


    // Look for squares of one unknown and three black cells, or two unknown and two black cells.

    for (int x = 0; x < m_width - 1; ++x) {
        for (int y = 0; y < m_height - 1; ++y) {
            struct XYState {
                int x;
                int y;
                State state;
            };

            array<XYState, 4> a = { {
                { x, y, cell(x, y) },
                { x + 1, y, cell(x + 1, y) },
                { x, y + 1, cell(x, y + 1) },
                { x + 1, y + 1, cell(x + 1, y + 1) }
            } };

            static_assert(UNKNOWN < BLACK, "This code assumes that UNKNOWN < BLACK.");

            sort(a.begin(), a.end(), [](const XYState& l, const XYState& r) {
                return l.state < r.state;
            });

            if (a[0].state == UNKNOWN
                && a[1].state == BLACK
                && a[2].state == BLACK
                && a[3].state == BLACK) {

                mark_as_white.insert(make_pair(a[0].x, a[0].y));

            } else if (a[0].state == UNKNOWN
                && a[1].state == UNKNOWN
                && a[2].state == BLACK
                && a[3].state == BLACK) {

                for (int i = 0; i < 2; ++i) {
                    set<pair<int, int>> imagine_black;

                    imagine_black.insert(make_pair(a[0].x, a[0].y));

                    if (unreachable(a[1].x, a[1].y, imagine_black)) {
                        mark_as_white.insert(make_pair(a[0].x, a[0].y));
                    }

                    std::swap(a[0], a[1]);
                }
            }
        }
    }

    if (process(verbose, mark_as_black, mark_as_white, "Whitened cells to prevent pools.")) {
        return m_sitrep;
    }


    // Look for isolated unknown regions.

    {
        const bool any_black_regions = any_of(m_regions.begin(), m_regions.end(),
            [](const shared_ptr<Region>& r) { return r->black(); });

        set<pair<int, int>> analyzed;

        for (int x = 0; x < m_width; ++x) {
            for (int y = 0; y < m_height; ++y) {
                if (cell(x, y) == UNKNOWN && analyzed.find(make_pair(x, y)) == analyzed.end()) {
                    bool encountered_black = false;

                    set<pair<int, int>> open;
                    set<pair<int, int>> closed;

                    open.insert(make_pair(x, y));

                    while (!open.empty()) {
                        const pair<int, int> p = *open.begin();
                        open.erase(open.begin());

                        switch (cell(p.first, p.second)) {
                            case UNKNOWN:
                                if (closed.insert(p).second) {
                                    insert_valid_neighbors(open, p.first, p.second);
                                }

                                break;

                            case BLACK:
                                encountered_black = true;
                                break;

                            default:
                                break;
                        }
                    }

                    if (!encountered_black && (
                        any_black_regions || static_cast<int>(closed.size()) < m_total_black)) {

                        mark_as_white.insert(closed.begin(), closed.end());
                    }

                    analyzed.insert(closed.begin(), closed.end());
                }
            }
        }
    }

    if (process(verbose, mark_as_black, mark_as_white, "Isolated unknown regions found.")) {
        return m_sitrep;
    }


    // A region would be "confined" if it could not be completed.
    // Black regions need to consume m_total_black cells.
    // White regions need to escape to a number.
    // Numbered regions need to consume N cells.

    // Confinement analysis consists of imagining what would happen if a particular unknown cell
    // were black or white. If that would cause any region to be confined, the unknown cell
    // must be the opposite color.

    // Black cells can't confine black regions, obviously.
    // Black cells can confine white regions, by isolating them.
    // Black cells can confine numbered regions, by confining them to an insufficiently large space.

    // White cells can confine black regions, by confining them to an insufficiently large space.
    //   (Humans look for isolation here, i.e. permanently separated black regions.
    //   That's harder for us to detect, but counting cells is similarly powerful.)
    // White cells can't confine white regions.
    //   (This is true for freestanding white cells, white cells added to other white regions,
    //   and white cells added to numbered regions.)
    // White cells can confine numbered regions, when added to other numbered regions.
    //   This is the most complicated case to analyze. For example:
    //   ####3
    //   #6 xXx
    //   #.  x
    //   ######
    //   Imagining cell 'X' to be white additionally prevents region 6 from consuming
    //   three 'x' cells. (This is true regardless of what other cells region 3 would
    //   eventually occupy.)

    for (int x = 0; x < m_width; ++x) {
        for (int y = 0; y < m_height; ++y) {
            if (cell(x, y) == UNKNOWN) {
                set<pair<int, int>> verboten;
                verboten.insert(make_pair(x, y));

                for (auto i = m_regions.begin(); i != m_regions.end(); ++i) {
                    const Region& r = **i;

                    if (confined(*i, cache, verboten)) {
                        if (r.black()) {
                            mark_as_black.insert(make_pair(x, y));
                        } else {
                            mark_as_white.insert(make_pair(x, y));
                        }
                    }
                }
            }
        }
    }

    for (auto i = m_regions.begin(); i != m_regions.end(); ++i) {
        const Region& r = **i;

        if (r.numbered() && r.size() < r.number()) {
            for (auto u = r.unk_begin(); u != r.unk_end(); ++u) {
                set<pair<int, int>> verboten;
                verboten.insert(*u);

                insert_valid_unknown_neighbors(verboten, u->first, u->second);

                for (auto k = m_regions.begin(); k != m_regions.end(); ++k) {
                    if (k != i && (*k)->numbered() && confined(*k, cache, verboten)) {
                        mark_as_black.insert(*u);
                    }
                }
            }
        }
    }

    if (process(verbose, mark_as_black, mark_as_white, "Confinement analysis succeeded.")) {
        return m_sitrep;
    }


    if (guessing) {
        vector<pair<int, int>> v;

        for (int x = 0; x < m_width; ++x) {
            for (int y = 0; y < m_height; ++y) {
                if (cell(x, y) == UNKNOWN) {
                    v.push_back(make_pair(x, y));
                }
            }
        }


        // Guess cells in a deterministic but pseudorandomized order.
        // This attempts to avoid repeatedly guessing cells that won't get us anywhere.

        auto dist = [this](const ptrdiff_t n) {
            // random_shuffle() provides n > 0. It wants [0, n).
            // uniform_int_distribution's ctor takes a and b with a <= b. It produces [a, b].
            return uniform_int_distribution<ptrdiff_t>(0, n - 1)(m_prng);
        };

        random_shuffle(v.begin(), v.end(), dist);


        for (auto u = v.begin(); u != v.end(); ++u) {
            const int x = u->first;
            const int y = u->second;

            for (int i = 0; i < 2; ++i) {
                const State color = i == 0 ? BLACK : WHITE;
                auto& mark_as_diff = i == 0 ? mark_as_white : mark_as_black;
                auto& mark_as_same = i == 0 ? mark_as_black : mark_as_white;

                Grid other(*this);

                other.mark(color, x, y);

                SitRep sr = KEEP_GOING;

                while (sr == KEEP_GOING) {
                    sr = other.solve(false, false);
                }

                if (sr == CONTRADICTION_FOUND) {
                    mark_as_diff.insert(make_pair(x, y));
                    process(verbose, mark_as_black, mark_as_white,
                        "Hypothetical contradiction found.");
                    return m_sitrep;
                }

                if (sr == SOLUTION_FOUND) {
                    mark_as_same.insert(make_pair(x, y));
                    process(verbose, mark_as_black, mark_as_white,
                        "Hypothetical solution found.");
                    return m_sitrep;
                }

                // sr == CANNOT_PROCEED
            }
        }
    }


    if (verbose) {
        print("I'm stumped!");
    }

    return CANNOT_PROCEED;
}

int Grid::known() const {
    int ret = 0;

    for (int x = 0; x < m_width; ++x) {
        for (int y = 0; y < m_height; ++y) {
            if (cell(x, y) != UNKNOWN) {
                ++ret;
            }
        }
    }

    return ret;
}

void Grid::write(ostream& os, const long long start, const long long finish) const {
    os <<
        "<!DOCTYPE html>\n"
        "<html>\n"
        "  <head>\n"
        "    <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" />\n"
        "    <style type=\"text/css\">\n"
        "      body {\n"
        "        font-family: Verdana, sans-serif;\n"
        "        line-height: 1.4;\n"
        "      }\n"
        "      table {\n"
        "        border: solid 3px #000000;\n"
        "        border-collapse: collapse;\n"
        "      }\n"
        "      td {\n"
        "        border: solid 1px #000000;\n"
        "        text-align: center;\n"
        "        width: 20px;\n"
        "        height: 20px;\n"
        "      }\n"
        "      td.unknown   { background-color: #C0C0C0; }\n"
        "      td.white.new { background-color: #FFFF00; }\n"
        "      td.white.old { }\n"
        "      td.black.new { background-color: #008080; }\n"
        "      td.black.old { background-color: #808080; }\n"
        "      td.number    { }\n"
        "    </style>\n"
        "    <title>Nurikabe</title>\n"
        "  </head>\n"
        "  <body>\n";

    long long old_ctr = start;

    for (auto i = m_output.begin(); i != m_output.end(); ++i) {
        const string& s = get<0>(*i);
        const auto& v = get<1>(*i);
        const auto& updated = get<2>(*i);
        const long long ctr = get<3>(*i);

        os << s << " (" << format_time(old_ctr, ctr) << ")\n";

        old_ctr = ctr;

        os << "<table>\n";

        for (int y = 0; y < m_height; ++y) {
            os << "<tr>";

            for (int x = 0; x < m_width; ++x) {
                os << "<td class=\"";
                os << (updated.find(make_pair(x, y)) != updated.end() ? "new " : "old ");

                switch (v[x][y]) {
                    case UNKNOWN: os << "unknown\"> ";           break;
                    case WHITE:   os <<   "white\">.";           break;
                    case BLACK:   os <<   "black\">#";           break;
                    default:      os <<  "number\">" << v[x][y]; break;
                }

                os << "</td>";
            }

            os << "</tr>\n";
        }

        os << "</table><br/>\n";
    }

    os << "Total: " << format_time(start, finish) << "\n";

    os <<
        "  </body>\n"
        "</html>\n";
}

bool Grid::valid(const int x, const int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

Grid::State& Grid::cell(const int x, const int y) {
    return m_cells[x][y].first;
}

const Grid::State& Grid::cell(const int x, const int y) const {
    return m_cells[x][y].first;
}

shared_ptr<Grid::Region>& Grid::region(const int x, const int y) {
    return m_cells[x][y].second;
}

const shared_ptr<Grid::Region>& Grid::region(const int x, const int y) const {
    return m_cells[x][y].second;
}

void Grid::print(const string& s, const set<pair<int, int>>& updated) {
    vector<vector<State>> v(m_width, vector<State>(m_height));

    for (int x = 0; x < m_width; ++x) {
        for (int y = 0; y < m_height; ++y) {
            v[x][y] = cell(x, y);
        }
    }

    m_output.push_back(make_tuple(s, v, updated, counter()));
}

bool Grid::process(const bool verbose, const set<pair<int, int>>& mark_as_black,
    const set<pair<int, int>>& mark_as_white, const string& s) {

    if (mark_as_black.empty() && mark_as_white.empty()) {
        return false;
    }

    for (auto i = mark_as_black.begin(); i != mark_as_black.end(); ++i) {
        mark(BLACK, i->first, i->second);
    }

    for (auto i = mark_as_white.begin(); i != mark_as_white.end(); ++i) {
        mark(WHITE, i->first, i->second);
    }

    if (verbose) {
        set<pair<int, int>> updated(mark_as_black);
        updated.insert(mark_as_white.begin(), mark_as_white.end());

        string t = s;

        if (m_sitrep == CONTRADICTION_FOUND) {
            t += " (Contradiction found! Attempted to fuse two numbered regions"
                " or mark an already known cell.)";
        }

        print(t, updated);
    }

    return true;
}

template <typename F> void Grid::for_valid_neighbors(const int x, const int y, F f) const {
    if (valid(x - 1, y)) {
        f(x - 1, y);
    }

    if (valid(x + 1, y)) {
        f(x + 1, y);
    }

    if (valid(x, y - 1)) {
        f(x, y - 1);
    }

    if (valid(x, y + 1)) {
        f(x, y + 1);
    }
}

void Grid::insert_valid_neighbors(set<pair<int, int>>& s, const int x, const int y) const {
    for_valid_neighbors(x, y, [&](const int a, const int b) {
        s.insert(make_pair(a, b));
    });
}

void Grid::insert_valid_unknown_neighbors(set<pair<int, int>>& s, const int x, const int y) const {
    for_valid_neighbors(x, y, [&](const int a, const int b) {
        if (cell(a, b) == UNKNOWN) {
            s.insert(make_pair(a, b));
        }
    });
}

void Grid::add_region(const int x, const int y) {
    // Construct a region, then add it to the cell and the set of all regions.

    set<pair<int, int>> unknowns;
    insert_valid_unknown_neighbors(unknowns, x, y);

    auto r = make_shared<Region>(cell(x, y), x, y, unknowns);

    region(x, y) = r;

    m_regions.insert(r);
}

void Grid::mark(const State s, const int x, const int y) {
    if (s != WHITE && s != BLACK) {
        throw logic_error("LOGIC ERROR: Grid::mark() - s must be either WHITE or BLACK.");
    }

    // If we're asked to mark an already known cell, we've encountered a contradiction.
    // Remember this, so that solve() can report the contradiction.

    if (cell(x, y) != UNKNOWN) {
        m_sitrep = CONTRADICTION_FOUND;
        return;
    }

    // Set the cell's new state. Because it's now known,
    // update each region's set of surrounding unknown cells.

    cell(x, y) = s;

    for (auto i = m_regions.begin(); i != m_regions.end(); ++i) {
        (*i)->unk_erase(x, y);
    }

    // Marking a cell as white or black could create an independent region,
    // could be added to an existing region, or could connect 2, 3, or 4 separate regions.
    // The easiest thing to do is to create a region for this cell,
    // and then fuse it to any adjacent compatible regions.

    add_region(x, y);

    // Don't attempt to cache these regions.
    // Each fusion could change this cell's region or its neighbors' regions.

    for_valid_neighbors(x, y, [this, x, y](const int a, const int b) {
        fuse_regions(region(x, y), region(a, b));
    });
}

// Note that r1 and r2 are passed by modifiable value. It's convenient to be able to swap them.
void Grid::fuse_regions(shared_ptr<Region> r1, shared_ptr<Region> r2) {

    // If we don't have two different regions, we're done.

    if (!r1 || !r2 || r1 == r2) {
        return;
    }

    // If we're asked to fuse two numbered regions, we've encountered a contradiction.
    // Remember this, so that solve() can report the contradiction.

    if (r1->numbered() && r2->numbered()) {
        m_sitrep = CONTRADICTION_FOUND;
        return;
    }

    // Black regions can't be fused with non-black regions.

    if (r1->black() != r2->black()) {
        return;
    }

    // We'll use r1 as the "primary" region, to which r2's cells are added.
    // It would be efficient to process as few cells as possible.
    // Therefore, we'd like to use the bigger region as the primary region.

    if (r2->size() > r1->size()) {
        std::swap(r1, r2);
    }

    // However, if the secondary region is numbered, then the primary region
    // must be white, so we need to swap them, even if the numbered region is smaller.

    if (r2->numbered()) {
        std::swap(r1, r2);
    }

    // Fuse the secondary region into the primary region.

    r1->insert(r2->begin(), r2->end());
    r1->unk_insert(r2->unk_begin(), r2->unk_end());

    // Update the secondary region's cells to point to the primary region.

    for (auto i = r2->begin(); i != r2->end(); ++i) {
        region(i->first, i->second) = r1;
    }

    // Erase the secondary region from the set of all regions.
    // When this function returns, the secondary region will be destroyed.

    m_regions.erase(r2);
}

bool Grid::impossibly_big_white_region(const int n) const {
    // A white region of N cells is impossibly big
    // if it could never be connected to any numbered region.

    return none_of(m_regions.begin(), m_regions.end(),
        [n](const shared_ptr<Region>& p) {
            // Add one because a bridge would be needed.
            return p->numbered() && p->size() + n + 1 <= p->number();
        });
}

// The cell at (x_root, y_root) is unreachable if it is unknown
// and it cannot be connected to a numbered region or a white region.
// An unknown cell completely surrounded by black cells is obviously unreachable,
// but because unreachability takes distance into account, it is considerably more powerful.
// We use a breadth-first search to discover the shortest path from the root to a numbered region
// or a white region. (Essentially, we're imagining a chain of white cells starting from the root.)
// We refuse to consider stepping anywhere that would join two numbered regions.
// Additionally, while we want to connect to a numbered region, we can't create a region that's
// too big for its number. (We'll add up the number of white cells that it took us to get from
// the root to the potential connection point, the current size of the numbered region,
// and the sizes of any white regions (up to 2) that would also be connected.)
// Finally, while we'd like to connect to a white region, we can't create
// an impossibly big white region.
// Note that this supersedes complete island analysis and forbidden bridge analysis.
// The "discovered" parameter can be used to tell unreachable() to avoid stepping on a cell.
// This is used when identifying squares of two unknown and two black cells;
// if imagining cell A to be black makes cell B unreachable, then cell A must be white.
bool Grid::unreachable(const int x_root, const int y_root, set<pair<int, int>> discovered) const {

    // We're interested in unknown cells.

    if (cell(x_root, y_root) != UNKNOWN) {
        return false;
    }

    // See https://en.wikipedia.org/wiki/Breadth-first_search

    queue<tuple<int, int, int>> q;

    q.push(make_tuple(x_root, y_root, 1));
    discovered.insert(make_pair(x_root, y_root));

    while (!q.empty()) {
        const int x_curr = get<0>(q.front());
        const int y_curr = get<1>(q.front());
        const int n_curr = get<2>(q.front());
        q.pop();

        set<shared_ptr<Region>> white_regions;
        set<shared_ptr<Region>> numbered_regions;

        for_valid_neighbors(x_curr, y_curr, [&](const int x, const int y) {
            const shared_ptr<Region>& r = region(x, y);

            if (r && r->white()) {
                white_regions.insert(r);
            } else if (r && r->numbered()) {
                numbered_regions.insert(r);
            }
        });

        int size = 0;

        for (auto i = white_regions.begin(); i != white_regions.end(); ++i) {
            size += (*i)->size();
        }

        for (auto i = numbered_regions.begin(); i != numbered_regions.end(); ++i) {
            size += (*i)->size();
        }

        if (numbered_regions.size() > 1) {
            continue;
        }

        if (numbered_regions.size() == 1) {
            const int num = (*numbered_regions.begin())->number();

            if (n_curr + size <= num) {
                return false;
            } else {
                continue;
            }
        }

        if (!white_regions.empty()) {
            if (impossibly_big_white_region(n_curr + size)) {
                continue;
            } else {
                return false;
            }
        }

        for_valid_neighbors(x_curr, y_curr, [&](const int x, const int y) {
            if (cell(x, y) == UNKNOWN && discovered.insert(make_pair(x, y)).second) {
                q.push(make_tuple(x, y, n_curr + 1));
            }
        });
    }

    return true;
}

// Is r confined, assuming that we can't consume verboten cells?
bool Grid::confined(const shared_ptr<Region>& r,
    map<shared_ptr<Region>, set<pair<int, int>>>& cache,
    const set<pair<int, int>>& verboten) const {

    // When we look for contradictions, we run confinement analysis (A) without verboten cells.
    // This gives us an opportunity to accelerate later confinement analysis (B)
    // when we have verboten cells.
    // During A, we'll record the unknown cells that we consumed.
    // During B, if none of the verboten cells are ones that we consumed,
    // then the verboten cells can't confine us.

    if (!verboten.empty()) {
        const auto i = cache.find(r);

        if (i == cache.end()) {
            return false; // We didn't consume any unknown cells.
        }

        const auto& consumed = i->second;

        if (none_of(verboten.begin(), verboten.end(), [&](const pair<int, int>& p) {
            return consumed.find(p) != consumed.end();
        })) {
            return false;
        }
    }

    // The open set contains cells that we're considering adding to the region.
    set<pair<int, int>> open(r->unk_begin(), r->unk_end());

    // The closed set contains cells that we've hypothetically added to the region.
    set<pair<int, int>> closed(r->begin(), r->end());

    // While we have cells to consider and we need to consume more cells...
    while (!open.empty()
        && (r->black() && static_cast<int>(closed.size()) < m_total_black
            || r->white()
            || r->numbered() && static_cast<int>(closed.size()) < r->number())) {

        // Consider cell p.
        const pair<int, int> p = *open.begin();
        open.erase(open.begin());

        // If it's verboten or we've already consumed it, discard it.
        if (verboten.find(p) != verboten.end() || closed.find(p) != closed.end()) {
            continue;
        }

        // We need to compare our region r with p's region (if any).
        const auto& area = region(p.first, p.second);

        if (r->black()) {
            if (!area) {
                // Keep going. A black region can consume an unknown cell.
            } else if (area->black()) {
                // Keep going. A black region can consume another black region.
            } else { // area->white() || area->numbered()
                continue; // We can't consume this. Discard it.
            }
        } else if (r->white()) {
            if (!area) {
                // Keep going. A white region can consume an unknown cell.
            } else if (area->black()) {
                continue; // We can't consume this. Discard it.
            } else if (area->white()) {
                // Keep going. A white region can consume another white region.
            } else { // area->numbered()
                return false; // Yay! Our region r escaped to a numbered region.
            }
        } else { // r->numbered()
            if (!area) {
                // A numbered region can't consume an unknown cell
                // that's adjacent to another numbered region.
                bool rejected = false;

                for_valid_neighbors(p.first, p.second, [&](const int x, const int y) {
                    const auto& other = region(x, y);

                    if (other && other->numbered() && other != r) {
                        rejected = true;
                    }
                });

                if (rejected) {
                    continue;
                }

                // Keep going. This unknown cell is okay to consume.
            } else if (area->black()) {
                continue; // We can't consume this. Discard it.
            } else if (area->white()) {
                // Keep going. A numbered region can consume a white region.
            } else { // area->numbered()
                throw logic_error("LOGIC ERROR: Grid::confined() - "
                    "I was confused and thought two numbered regions would be adjacent.");
            }
        }

        if (!area) { // Consume an unknown cell.
            closed.insert(p);
            insert_valid_neighbors(open, p.first, p.second);

            if (verboten.empty()) {
                cache[r].insert(p);
            }
        } else { // Consume a whole region.
            closed.insert(area->begin(), area->end());
            open.insert(area->unk_begin(), area->unk_end());
        }
    }

    // We're confined if we still need to consume more cells.
    return r->black() && static_cast<int>(closed.size()) < m_total_black
        || r->white()
        || r->numbered() && static_cast<int>(closed.size()) < r->number();
}

string Grid::detect_contradictions(map<shared_ptr<Region>, set<pair<int, int>>>& cache) const {
    for (int x = 0; x < m_width - 1; ++x) {
        for (int y = 0; y < m_height - 1; ++y) {
            if (cell(x, y) == BLACK
                && cell(x + 1, y) == BLACK
                && cell(x, y + 1) == BLACK
                && cell(x + 1, y + 1) == BLACK) {

                return "Contradiction found! Pool detected.";
            }
        }
    }

    int black_cells = 0;
    int white_cells = 0;

    for (auto i = m_regions.begin(); i != m_regions.end(); ++i) {
        const Region& r = **i;

        // We don't need to look for gigantic black regions because
        // counting black cells is strictly more powerful.

        if (r.white() && impossibly_big_white_region(r.size())
            || r.numbered() && r.size() > r.number()) {

            return "Contradiction found! Gigantic region detected.";
        }

        (r.black() ? black_cells : white_cells) += r.size();


        if (confined(*i, cache)) {
            return "Contradiction found! Confined region detected.";
        }
    }

    if (black_cells > m_total_black) {
        return "Contradiction found! Too many black cells detected.";
    }

    if (white_cells > m_width * m_height - m_total_black) {
        return "Contradiction found! Too many white/numbered cells detected.";
    }

    return "";
}

Grid::Grid(const Grid& other)
    : m_width(other.m_width),
    m_height(other.m_height),
    m_total_black(other.m_total_black),
    m_cells(other.m_cells),
    m_regions(),
    m_sitrep(other.m_sitrep),
    m_output(other.m_output),
    m_prng(other.m_prng) {

    for (auto i = other.m_regions.begin(); i != other.m_regions.end(); ++i) {
        m_regions.insert(make_shared<Region>(**i));
    }

    for (auto i = m_regions.begin(); i != m_regions.end(); ++i) {
        for (auto k = (*i)->begin(); k != (*i)->end(); ++k) {
            region(k->first, k->second) = *i;
        }
    }
}

Grid& Grid::operator=(const Grid& other) {
    Grid(other).swap(*this);
    return *this;
}

void Grid::swap(Grid& other) {
    std::swap(m_width, other.m_width);
    std::swap(m_height, other.m_height);
    std::swap(m_total_black, other.m_total_black);
    std::swap(m_cells, other.m_cells);
    std::swap(m_regions, other.m_regions);
    std::swap(m_sitrep, other.m_sitrep);
    std::swap(m_output, other.m_output);
    std::swap(m_prng, other.m_prng);
}
