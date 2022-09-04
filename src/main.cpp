#include <gtest/gtest.h>
#include <variant>
#include <stack>
#include <vector>
#include <span>

using FileItemVariant = std::variant<class Drive, class File, class Directory>;
using Path = std::vector<int>;
struct NonExist : std::runtime_error {NonExist():std::runtime_error("Does not exist"){}};
struct CannotRename : std::runtime_error {CannotRename():std::runtime_error("Cannot rename"){}};

class FileItem;


class NamedFileItem
{
    std::string m_name;
public:
    NamedFileItem() = default;
    NamedFileItem(std::string_view name):m_name(name) {}

    std::string_view GetName() const { return std::string_view{m_name}; }
    void SetName(std::string_view n) { m_name = n; }
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
    FileItem& Get(int idx) { if(idx<0 || idx>=(int)m_contents.size()) throw NonExist{}; return m_contents[idx];}
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



class Directory : public ContainerFileItem, public NamedFileItem
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
                fn(item, path);
                Recurse(std::forward<Fn>(fn), item, path);
            }, as_variant);
    }
    // process
    template<class Fn, class FileItemType>
    void Recurse(Fn&& fn, const FileItemType& item, Path path) const
    {
        if constexpr (std::is_base_of_v<ContainerFileItem, FileItemType>)
        {
            item.Visit([&fn, path, i=0] (const FileItem& fi) mutable
            {
                path.push_back(i++);
                fi.Recurse(std::forward<Fn>(fn), path);
                path.pop_back();
            });
        }
    }
    // access
    template<class FileItemType>
    FileItem& Get(FileItemType& fi, int idx)
    {
        if constexpr (std::is_base_of_v<ContainerFileItem, FileItemType>)
            return fi.Get(idx);
        else
            throw NonExist{};
    }
    // operations
    template<class FileItemType>
    void Rename(FileItemType& fi, std::string_view new_name)
    {
        if constexpr (std::is_base_of_v<NamedFileItem, FileItemType>)
            fi.SetName(new_name);
        else
            throw CannotRename{};
    }
public:
    using FileItemVariant::FileItemVariant;
 
    template<class Fn>
    void Recurse(Fn&& fn) const
    {
        Recurse(std::forward<Fn>(fn), Path{});
    }
    void Rename(std::string_view new_name)
    {
        auto& as_variant = static_cast<FileItemVariant&>(*this);
        std::visit(
            [this, new_name](auto& item) {
                Rename(item, new_name);
            }, as_variant);
    }
    FileItem& operator[](int idx)
    {
        auto& as_variant = static_cast<FileItemVariant&>(*this);
        return std::visit(
            [this, idx](auto& item)->FileItem& {
                return Get(item, idx);
            }, as_variant);
    }
    FileItem& operator[](const Path& path)
    {
        auto remaining = std::span<const int>(path.data(), path.size());
        FileItem* current = this;
        while (!remaining.empty())
        {
            current = &(*current)[remaining[0]];
            remaining = remaining.subspan(1);
        }
        return *current;
    }
    friend std::ostream& operator<<(std::ostream&, const FileItem&);
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
    auto dir = std::get<Directory>(drive_a[0]);
    ASSERT_THROW(drive_a[1], NonExist);
    ASSERT_STREQ(dir.GetName().data(), "Animals");
    const auto file = std::get<File>(dir.Get(0));
    ASSERT_STREQ(file.GetName().data(), "Aardvark");
}


TEST(FileItem,GetPath)
{
    File animal_file{"Aardvark"};
    Directory animal_files{"Animals", 
        {animal_file}
    };
    FileItem drive_a = Drive{'a', 
        {animal_files}
    };
    const auto dir = std::get<Directory>(drive_a[Path{0}]);
    ASSERT_STREQ(dir.GetName().data(), "Animals");
    ASSERT_THROW(drive_a[Path{1}], NonExist);
    const auto& file = std::get<File>(drive_a[Path{0,0}]);
    ASSERT_STREQ(file.GetName().data(), "Aardvark");
    ASSERT_THROW((drive_a[Path{0,1}]), NonExist);

    drive_a[Path{0,0}].Rename("Antalope");
    ASSERT_STREQ(file.GetName().data(), "Antalope");
}
