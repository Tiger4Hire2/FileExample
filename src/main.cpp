#include <gtest/gtest.h>
#include <variant>
#include <stack>
#include <vector>
#include <span>

using FileItem = std::variant<class Drive, class File, class Directory>;
using Path = std::vector<int>;
using PartialPath = std::span<int>;
struct NonExist : std::runtime_error {NonExist():std::runtime_error("Does not exist"){}};
class NamedFileItem
{
    std::string m_name;
public:
    NamedFileItem() = default;
    NamedFileItem(std::string_view name):m_name(name) {}

    std::string_view GetName() const { return std::string_view{m_name}; }
};

template<class Derived>
class ContainerFileItem
{
    std::vector<FileItem>   m_contents;
public:
    ContainerFileItem() = default;
    ContainerFileItem(std::initializer_list<FileItem> list): m_contents(list) {}
    template<class Fn>
    void Recurse(Fn&& fn, Path path)
    {
        fn(static_cast<Derived&>(*this), path);
        for (size_t i = 0; i < m_contents.size(); ++i)
        {
            path.push_back(i);
            std::visit([&fn](auto& i){i.Recurse(std::forward<Fn>(fn));}, m_contents.at(i));
            path.pop_back(); 
        }
    }

    template<class Fn>
    void Recurse(Fn&& fn, Path path) const
    {
        fn(static_cast<const Derived&>(*this), path);
        for (size_t i = 0; i < m_contents.size(); ++i)
        {
            path.push_back(i);
            std::visit([&fn, &path](auto& item){item.Recurse(std::forward<Fn>(fn), path);}, m_contents.at(i));
            path.pop_back(); 
        }
    }

    const FileItem& Get(int idx) const { return m_contents.at(idx); }
    FileItem& Get(int idx) { return m_contents.at(idx); }
    size_t GetNumItems(int idx) const { return m_contents.size(); }
    const FileItem& GetPath(PartialPath path) const
    {
        if (path.size()==1)
            return m_contents.at(path[0]);
        else
            return std::visit(
                [&path](const auto& obj) -> const FileItem& {
                    return obj.GetPath(path.subspan(1));
                }, m_contents.at(path[0]));
    }



    void Add(const FileItem& item) {m_contents.emplace_back(item);}
    // warning removing always invalidates all paths.
//    void Remove(int idx) {m_contents.erase(m_contents.begin()+idx);}
};

class Drive : public ContainerFileItem<Drive>
{
    char                    m_drive_letter{'a'};
public:
    Drive(char id) : m_drive_letter(id) {}
    Drive(char id, std::initializer_list<FileItem> contents) 
        : m_drive_letter(id)
        , ContainerFileItem<Drive>(contents)
        {}

    std::string_view GetName() const { return std::string_view{&m_drive_letter, 1}; }
};



class Directory : public NamedFileItem, public ContainerFileItem<Directory>
{
public:
    Directory() = default;
    Directory(std::string_view name) : NamedFileItem(name), ContainerFileItem<Directory>({}) {}
    Directory(std::string_view name, std::initializer_list<FileItem> list) : NamedFileItem(name), ContainerFileItem<Directory>(list) {}
};

class File : public NamedFileItem
{
public:
    using NamedFileItem::NamedFileItem;

    template<class Fn>
    void Recurse(Fn&& fn, Path path)
    {
        fn(*this, path);
    }

    template<class Fn>
    void Recurse(Fn&& fn, Path path) const
    {
        fn(*this, path);
    }
    const FileItem& Get(int idx) const { throw NonExist{}; }
    FileItem& Get(int idx) { throw NonExist{}; }
    const FileItem& GetPath(PartialPath) const { throw NonExist{}; }
    FileItem& Get(PartialPath) { throw NonExist{}; }
};


void PrintContents(const FileItem& dir)
{
    const auto Print = [](const auto& item, Path path)
    {
        for (size_t i = 0; i < path.size(); ++i)
            std::cout << "\t";
        std::cout << item.GetName();
        std::cout << "\n";
    };
    std::visit([&Print](const auto& i){i.Recurse(Print, Path{});}, dir); 
}



TEST(FileItem,Get)
{
    File animal_file{"Aardvark"};
    Directory animal_files{"Animals", 
        {animal_file}
    };
    Drive drive_a{'a', 
        {animal_files}};
    PrintContents(drive_a);
    const auto dir = std::get<Directory>(drive_a.Get(0));
    ASSERT_STREQ(dir.GetName().data(), "Animals");
    Path path{0,0};
    const auto file = std::get<File>(dir.Get(0));
    ASSERT_STREQ(file.GetName().data(), "Aardvark");
}

TEST(FileItem,GetPath)
{
    File animal_file{"Aardvark"};
    Directory animal_files{"Animals", 
        {animal_file}
    };
    Drive drive_a{'a',
        {animal_files}};
    Path path{0,0};
    const auto dir = std::get<Directory>(drive_a.GetPath(std::span(path.data(), 1)));
    ASSERT_STREQ(dir.GetName().data(), "Animals");
    const auto file = std::get<File>(drive_a.GetPath(std::span(path.data(), 2)));
    ASSERT_STREQ(file.GetName().data(), "Aardvark");
}