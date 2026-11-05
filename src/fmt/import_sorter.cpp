#include <keety/fmt/import_sorter.hpp>
#include <keety/resolver/module_path_resolver.hpp>

#include <algorithm>

namespace keety::fmt {

ImportTier ImportSorter::classify(std::string_view modulePath) noexcept {
    // 1. Local relative imports starting with ./ or ../ (or .\ or ..\)
    if (modulePath.rfind("./", 0) == 0 || modulePath.rfind("../", 0) == 0 ||
        modulePath.rfind(".\\", 0) == 0 || modulePath.rfind("..\\", 0) == 0) {
        return ImportTier::LocalRelative;
    }

    // 2. Standard library module
    if (resolver::ModulePathResolver::isStandardLibraryModule(modulePath)) {
        return ImportTier::StandardLibrary;
    }

    // 3. Otherwise Third-Party
    return ImportTier::ThirdParty;
}

std::vector<std::vector<const ast::ImportDecl*>> ImportSorter::groupAndSort(
    const std::vector<const ast::ImportDecl*>& imports
) {
    std::vector<const ast::ImportDecl*> stdlib;
    std::vector<const ast::ImportDecl*> thirdParty;
    std::vector<const ast::ImportDecl*> local;

    for (const auto* imp : imports) {
        if (!imp) continue;
        ImportTier tier = classify(imp->getModulePath());
        switch (tier) {
        case ImportTier::StandardLibrary:
            stdlib.push_back(imp);
            break;
        case ImportTier::ThirdParty:
            thirdParty.push_back(imp);
            break;
        case ImportTier::LocalRelative:
            local.push_back(imp);
            break;
        }
    }

    auto sortFn = [](const ast::ImportDecl* a, const ast::ImportDecl* b) {
        if (a->getModulePath() != b->getModulePath()) {
            return a->getModulePath() < b->getModulePath();
        }
        if (a->getModuleAlias() != b->getModuleAlias()) {
            return a->getModuleAlias() < b->getModuleAlias();
        }
        return a->isReExported() < b->isReExported();
    };

    std::sort(stdlib.begin(), stdlib.end(), sortFn);
    std::sort(thirdParty.begin(), thirdParty.end(), sortFn);
    std::sort(local.begin(), local.end(), sortFn);

    return {std::move(stdlib), std::move(thirdParty), std::move(local)};
}

} // namespace keety::fmt
