#include <gtest/gtest.h>
#include <variant>
#include <stack>
#include <vector>
#include <span>

using FileItemVariant = std::variant<class Drive, class File, class Directory>;
using Path = std::vector<int>;
using PartialPath = std::span<int>;
struct NonExist : std::runtime_error {NonExist():std::runtime_error("Does not exist"){}};

class FileItem;


class NamedFileItem
{
    std::string m_name;
public:
    NamedFileItem() = default;
    NamedFileItem(std::string_view name):m_name(name) {}

    std::string_view GetName() const { return std::string_view{m_name}; }
};

class ContainerFileItem
{
    std::vector<FileItem>   m_contents;
public:
    ContainerFileItem() = default;
    ContainerFileItem(std::initializer_list<FileItem> list): m_contents(list) {}

    template<class Fn>
    void Visit(Fn&& fn)
    {
        for (auto& c:m_contents) fn(c);
    }
    template<class Fn>
    void Visit(Fn&& fn) const
    {
        for (const auto& c:m_contents) fn(c);
    }
};

class Drive : public ContainerFileItem
{
    char                    m_drive_letter{'a'};
public:
    Drive(char id) : m_drive_letter(id) {}
    Drive(char id, std::initializer_list<FileItem> contents) 
        : m_drive_letter(id)
        , ContainerFileItem(contents)
        {}

    std::string_view GetName() const { return std::string_view{&m_drive_letter, 1}; }
};



class Directory : public NamedFileItem, public ContainerFileItem
{
public:
    Directory() = default;
    Directory(std::string_view name) : NamedFileItem(name), ContainerFileItem({}) {}
    Directory(std::string_view name, std::initializer_list<FileItem> list) : NamedFileItem(name), ContainerFileItem(list) {}
};

class File : public NamedFileItem
{
public:
    using NamedFileItem::NamedFileItem;
};


class FileItem : public FileItemVariant
{
    // dispatch
    template<class Fn>
    void Recurse(Fn&& fn, Path path) const
    {
        const auto as_variant = static_cast<const FileItemVariant&>(*this);
        std::visit(
            [this,&fn, &path](const auto& item) {
                Recurse(std::forward<Fn>(fn), item, path);
            }, as_variant);
    }
    // process
    template<class Fn>
    void Recurse(Fn&& fn, const Drive& item, Path path) const
    {
        fn(item, path);
        Recurse(std::forward<Fn>(fn), static_cast<const ContainerFileItem&>(item), path);
    }
    template<class Fn>
    void Recurse(Fn&& fn, const Directory& item, Path path) const
    {
        fn(item, path);
        Recurse(std::forward<Fn>(fn), static_cast<const ContainerFileItem&>(item), path);
    }
    template<class Fn>
    void Recurse(Fn&& fn, const File& item, Path path) const
    {
        fn(item, path);
    }

    template<class Fn>
    void Recurse(Fn&& fn, const ContainerFileItem& item, Path path) const
    {
        item.Visit([&fn, path, i=0] (const FileItem& fi) mutable
        {
            path.push_back(i++);
            fi.Recurse(std::forward<Fn>(fn), path);
            path.pop_back();
        });
    }

public:
    template<class Fn>
    void Recurse(Fn&& fn) const
    {
        Recurse(std::forward<Fn>(fn), Path{});
    }
    friend std::ostream& operator<<(std::ostream&, const FileItem&);
    using FileItemVariant::FileItemVariant;
};

inline std::ostream& operator<<(std::ostream& os, const FileItem& item)
{
    const auto print_fn = [&os](const auto& fi, Path path)
    {
        for (int ignored: path)
            std::cout << "\t";
        os<<fi.GetName()<<"\n";
    };
    item.Recurse(print_fn);
    return os;
}

TEST(FileItem,Get)
{
    File animal_file{"Aardvark"};
    Directory animal_files{"Animals", 
        {FileItem{animal_file}}
    };
    FileItem drive_a = Drive{'a', 
        {animal_files}
    };
    std::cout << drive_a;
//    PrintContents(drive_a);
//    const auto dir = std::get<Directory>(drive_a.Get(0));
//    ASSERT_STREQ(dir.GetName().data(), "Animals");
//    Path path{0,0};
//    const auto file = std::get<File>(dir.Get(0));
//    ASSERT_STREQ(file.GetName().data(), "Aardvark");
}

/*
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
*/