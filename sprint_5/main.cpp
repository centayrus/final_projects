#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
// #include <thread>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char *data, std::size_t sz) {
    return path(data, data + sz);
}
path LocalDirSearch(const path &parent, const path &inc_path) {
    error_code err;
    path find_path, empty_path;
    for (const auto &dir_entry : filesystem::directory_iterator(parent / inc_path.parent_path(), err)) { //
        path p = inc_path.filename().string();
        if (dir_entry.path().filename().string() == p && dir_entry.status().type() == filesystem::file_type::regular) {
            find_path = dir_entry.path();
            ifstream in_check(dir_entry.path());
            if (!in_check) {
                return empty_path;
            } else {
                return find_path;
            }
        }
    }
    return empty_path;
}

// поиск по элементам вектора
path VectorDirSearch(const path &inc_path, const vector<path> &include_directories) {
    error_code err;
    path find_path, empty_path;
    for (const auto &v_path : include_directories) {
        for (const auto &dir_entry : filesystem::directory_iterator(v_path / inc_path.parent_path(), err)) { //
            if (dir_entry.path().filename().string() == inc_path.filename().string() && dir_entry.status().type() == filesystem::file_type::regular) {
                find_path = dir_entry.path();
                ifstream in_check(dir_entry.path());
                if (!in_check) {
                    return empty_path;
                } else {
                    return find_path;
                }
            }
        }
    }
    return empty_path;
}

bool PreprocessImpl(const path &in_file, ofstream &out, const path &cur_file, const vector<path> &include_directories) {
    smatch m;
    string str;
    path parent = in_file.parent_path();
    path inc_path;
    static const regex includ_file(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static const regex includ_lib(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    ifstream in(in_file);
    // ожидание открытия исходного файла
    if (!in) {
        throw runtime_error("Файл не открылся на чтение"s);
    }
    int line = 0;
    while (getline(in, str)) {
        ++line;
        if (regex_match(str, m, includ_file)) {
            inc_path = string(m[1]);
            path local_path = LocalDirSearch(parent, inc_path);
            if (!local_path.empty()) {
                if (PreprocessImpl(parent / inc_path, out, in_file, include_directories)) {
                    continue;
                } else {
                    return false;
                }
            } else {
                path vector_path = VectorDirSearch(inc_path, include_directories);
                if (!vector_path.empty()) {
                    if (PreprocessImpl(vector_path, out, in_file, include_directories)) {
                        continue;
                    } else {
                        return false;
                    }
                }
            }
        } else if (regex_match(str, m, includ_lib)) {
            inc_path = string(m[1]);
            path vector_path = VectorDirSearch(inc_path, include_directories);
            if (!vector_path.empty()) {
                if (PreprocessImpl(vector_path, out, in_file, include_directories)) {
                    continue;
                } else {
                    return false;
                }
            }
        } else {
            out << str << endl;
            continue;
        }
        cout << "unknown include file "s << inc_path.filename().string() << " at file "s << cur_file.string() << " at line "s << line << endl;
        return false;
    }
    return true;
}

bool Preprocess(const path &in_file, const path &out_file, const vector<path> &include_directories) {
    // в условии открытие на запись должно быть после успешного открытия на чтение,
    ifstream in(in_file);
    if (!in) {
        return false;
    }
    // выбрасываем исключение, если поток на запись не открывается
    ofstream out(out_file);
    if (!out) {
        throw runtime_error("Файл не открылся на чтение"s);
    }
    // если подавать в рекурсивную функцию потоков чтения, то, думаю, это увеличит количество строк кода,
    // поскольку потребует перед каждым вызовом рекурсивной функции открывать поток на чтение,
    // а вот поток записи можно подавать по ссылке. Так и сделал
    return PreprocessImpl(in_file, out, in_file, include_directories);
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
    Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p, {"sources"_p / "include1"_p, "sources"_p / "include2"_p});
    //                      {"sources"_p / "include1"_p, "sources"_p / "include2"_p});
    // assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,{"sources"_p / "include1"_p, "sources"_p / "include2"_p})));

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
    GetFileContents("sources/a.in"s);
    //  assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}

// for (const auto &dir_entry : filesystem::directory_iterator(parent / inc_path.parent_path())) {
//     path p = inc_path.filename().string();
//     if (dir_entry.path().filename().string() == p && dir_entry.status().type() == filesystem::file_type::regular) {
//         is_find_file = true;
//         out.close();
//         Preprocess(parent / inc_path, out_file, inc_path.filename().string(), include_directories);
//         break;
//     }
// }