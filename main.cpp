#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(ifstream& in_file, ofstream& out_file, const path& in_file_path, const vector<path>& include_directories) {
    static regex user_include(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex default_include(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch m;

    string line;
    while (getline(in_file, line)) {
        if (regex_match(line, m, default_include)) {
            path p = string(m[1]);
            cout << p.string() << endl;

            ifstream include_in_file;
            include_in_file.close();
            vector<path> sub_dirs;
            for (const auto& dir_path : include_directories) {   
                for (const auto& dir_entry : filesystem::directory_iterator(dir_path)) {
                    if (dir_entry.is_regular_file() && dir_entry.path().filename() == p.filename()) {
                        include_in_file.open(dir_entry.path());
                        Preprocess(include_in_file, out_file, dir_entry.path(), include_directories);
                    } else if (dir_entry.is_directory()) {
                        Preprocess(in_file, out_file, in_file_path, {dir_entry.path()});
                    }
                }
            }
            if (!include_in_file.is_open()) {
                cerr << __LINE__ << "\n";
                return false;
            }
        } else if (regex_match(line, m, user_include)) {
            path p = string(m[1]);
            cout << p.string() << endl;

            ifstream include_in_file;
            include_in_file.close();
            //поиск в род папке
            for (const auto& dir_entry : filesystem::directory_iterator(in_file_path.parent_path())) {
                cout << dir_entry.path().string() << endl;
                if (dir_entry.is_regular_file() && dir_entry.path().filename() == p.filename()) {
                    include_in_file.open(dir_entry.path());
                    Preprocess(include_in_file, out_file, dir_entry.path(), include_directories);
                } else if (dir_entry.is_directory()) {
                    Preprocess(in_file, out_file, in_file_path, {dir_entry.path()});
                }
            }
            //
            if (!include_in_file.is_open()) {
                for (const auto& dir_path : include_directories) {
                    cout << dir_path << endl;
                    for (const auto& dir_entry : filesystem::directory_iterator(dir_path)) {
                        if (dir_entry.is_regular_file() && dir_entry.path().filename() == p.filename()) {
                            include_in_file.open(dir_entry.path());
                            Preprocess(include_in_file, out_file, dir_entry.path(), include_directories);
                        } else if (dir_entry.is_directory()) {
                            Preprocess(in_file, out_file, in_file_path, {dir_entry.path()});
                        }
                    }
                }
            }
            if (!include_in_file.is_open()) {
                cerr << __LINE__ << "\n";
                return false;
            }
        } else {
            out_file << line << endl;
        }
    }
    return true;
}

// напишите эту функцию
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in_file_(in_file, ios::in);
    if (!in_file_.is_open()) {
        return false;
    }
    ofstream out_file_(out_file, ios::out);

    return Preprocess(in_file_, out_file_, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}