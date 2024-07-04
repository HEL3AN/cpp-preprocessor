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

bool FindIncludeFile(const string& include_filename, const vector<path>& include_directories, path& include_file) {
    for (const auto& dir : include_directories) {
        include_file = dir / include_filename;
        if (exists(include_file)) {
            return true;
        }
    }
    return false;
}

bool ProcessFile(const path& file_path, std::ostream& out_file, const vector<path>& include_directories, const path& current_directory) {
    ifstream in_file(file_path);
    if (!in_file.is_open()) {
        return false;
    }

    static regex user_include(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex default_include(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    
    string line;
    size_t line_number = 0;

    while(getline(in_file, line)) {
        ++line_number;
        smatch match;
        if (regex_match(line, match, user_include)) {
            path include_file = current_directory / match[1].str();
            if (!exists(include_file) && !FindIncludeFile(match[1].str(), include_directories, include_file)) {
                std::cout << "unknown include file " << match[1].str() << " at file " << file_path.string() << " at line " << line_number << std::endl;
                return false;
            }
            if (!ProcessFile(include_file, out_file, include_directories, include_file.parent_path())) {
                return false;
            }
        } else if (regex_match(line, match, default_include)) {
            path include_file = current_directory / match[1].str();
            if (!FindIncludeFile(match[1].str(), include_directories, include_file)) {
                std::cout << "unknown include file " << match[1].str() << " at file " << file_path.string() << " at line " << line_number << std::endl;
                return false;
            }
            if (!ProcessFile(include_file, out_file, include_directories, include_file.parent_path())) {
                return false;
            }
        } else {
            out_file << line << "\n";
        }
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in_stream(in_file, ios::in);
    if (!in_stream.is_open()) {
        return false;
    }
    ofstream out_stream(out_file, ios::out);
    if (!out_stream.is_open()) {
        return false;
    }

    return ProcessFile(in_file, out_stream, include_directories, in_file.parent_path()); 
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