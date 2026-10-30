#include <keety/ir/recursive_boxing.hpp>

namespace keety::ir {

void RecursiveBoxingPass::register_class(std::string name, std::vector<std::pair<std::string, typechecker::TypePtr>> fields) {
    class_fields_[name] = std::move(fields);

    // Recompute recursive fields across all registered classes
    recursive_field_keys_.clear();

    for (const auto& [cname, cfields] : class_fields_) {
        for (const auto& [fname, ftype] : cfields) {
            if (is_type_recursive(cname, ftype)) {
                recursive_field_keys_.insert(cname + "::" + fname);
            }
        }
    }
}

void RecursiveBoxingPass::scan_classes(const parser::ModuleAST& ast) {
    for (const auto& decl : ast.declarations) {
        if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            std::vector<std::pair<std::string, typechecker::TypePtr>> fields;
            for (const auto& f : cls->getFields()) {
                typechecker::TypePtr type = typechecker::Type::fromAnnotation(f.type.get());
                fields.emplace_back(f.name, std::move(type));
            }
            register_class(cls->getName(), std::move(fields));
        }
    }
}

bool RecursiveBoxingPass::has_path_to(const std::string& from, const std::string& target, std::unordered_set<std::string>& visited) const {
    if (from == target) {
        return true;
    }
    if (visited.find(from) != visited.end()) {
        return false;
    }
    visited.insert(from);

    auto it = class_fields_.find(from);
    if (it == class_fields_.end()) {
        return false;
    }

    for (const auto& [fname, ftype] : it->second) {
        if (!ftype) continue;
        if (ftype->getKind() == typechecker::TypeKind::Custom) {
            const auto* c = static_cast<const typechecker::CustomType*>(ftype.get());
            if (has_path_to(c->getName(), target, visited)) {
                return true;
            }
        } else if (ftype->getKind() == typechecker::TypeKind::Union) {
            const auto* u = static_cast<const typechecker::UnionType*>(ftype.get());
            for (const auto& v : u->getVariants()) {
                if (v && v->getKind() == typechecker::TypeKind::Custom) {
                    const auto* c = static_cast<const typechecker::CustomType*>(v.get());
                    if (has_path_to(c->getName(), target, visited)) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool RecursiveBoxingPass::is_type_recursive(const std::string& class_name, const typechecker::TypePtr& type) const {
    if (!type) {
        return false;
    }

    if (type->getKind() == typechecker::TypeKind::Custom) {
        const auto* c = static_cast<const typechecker::CustomType*>(type.get());
        if (c->getName() == class_name) {
            return true;
        }
        std::unordered_set<std::string> visited;
        return has_path_to(c->getName(), class_name, visited);
    }

    if (type->getKind() == typechecker::TypeKind::Union) {
        const auto* u = static_cast<const typechecker::UnionType*>(type.get());
        for (const auto& variant : u->getVariants()) {
            if (is_type_recursive(class_name, variant)) {
                return true;
            }
        }
    }

    return false;
}

bool RecursiveBoxingPass::is_recursive_field(const std::string& class_name, const std::string& field_name) const {
    return recursive_field_keys_.find(class_name + "::" + field_name) != recursive_field_keys_.end();
}

bool RecursiveBoxingPass::is_recursive_class(const std::string& class_name) const {
    std::string prefix = class_name + "::";
    for (const auto& key : recursive_field_keys_) {
        if (key.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<RecursiveFieldInfo> RecursiveBoxingPass::get_recursive_fields() const {
    std::vector<RecursiveFieldInfo> result;
    for (const auto& [cname, fields] : class_fields_) {
        for (const auto& [fname, ftype] : fields) {
            if (is_recursive_field(cname, fname)) {
                result.push_back({cname, fname, ftype});
            }
        }
    }
    return result;
}

std::string RecursiveBoxingPass::resolve_class_name(const Operand& obj, const std::string& member_name, const Function& fn) const {
    // 1. Direct type on operand
    if (obj.get_type() && obj.get_type()->getKind() == typechecker::TypeKind::Custom) {
        return static_cast<const typechecker::CustomType*>(obj.get_type().get())->getName();
    }

    // 2. Check function parameters
    for (const auto& param : fn.get_parameters()) {
        if (param.name == obj.get_name() && param.type &&
            param.type->getKind() == typechecker::TypeKind::Custom) {
            return static_cast<const typechecker::CustomType*>(param.type.get())->getName();
        }
    }

    // 3. Check AllocaInst
    for (const auto& bb : fn.get_blocks()) {
        if (!bb) continue;
        for (const auto& inst : bb->get_instructions()) {
            if (inst && inst->get_opcode() == Opcode::Alloca) {
                const auto& alloca = static_cast<const AllocaInst&>(*inst);
                if (alloca.get_name() == obj.get_name() && alloca.get_type() &&
                    alloca.get_type()->getKind() == typechecker::TypeKind::Custom) {
                    return static_cast<const typechecker::CustomType*>(alloca.get_type().get())->getName();
                }
            }
        }
    }

    // 4. Check ClassConstructInst producing obj
    for (const auto& bb : fn.get_blocks()) {
        if (!bb) continue;
        for (const auto& inst : bb->get_instructions()) {
            if (inst && inst->get_opcode() == Opcode::ClassConstruct) {
                const auto& cc = static_cast<const ClassConstructInst&>(*inst);
                if (cc.get_dest() == obj) {
                    return cc.get_class_name();
                }
            }
        }
    }

    // 5. Fallback heuristic: check which registered class has this field
    std::string matching_class;
    size_t matches = 0;
    for (const auto& [cname, fields] : class_fields_) {
        for (const auto& [fname, ftype] : fields) {
            if (fname == member_name) {
                matching_class = cname;
                ++matches;
                break;
            }
        }
    }
    if (matches == 1) {
        return matching_class;
    }

    return "";
}

size_t RecursiveBoxingPass::run_on_function(Function& fn) {
    size_t count = 0;

    for (const auto& bb : fn.get_blocks()) {
        if (!bb) continue;

        for (size_t i = 0; i < bb->get_instructions().size(); ++i) {
            const auto& inst = bb->get_instructions()[i];
            if (!inst) continue;

            switch (inst->get_opcode()) {
                case Opcode::MemberStore: {
                    if (!options_.auto_box_writes) break;
                    auto& store = static_cast<MemberStoreInst&>(*inst);
                    std::string cls_name = resolve_class_name(store.get_obj(), store.get_member(), fn);
                    if (!cls_name.empty() && is_recursive_field(cls_name, store.get_member())) {
                        // Check if previous instruction is already BoxInst for this value
                        bool already_boxed = false;
                        if (i > 0) {
                            const auto& prev = bb->get_instructions()[i - 1];
                            if (prev && prev->get_opcode() == Opcode::Box) {
                                const auto& prev_box = static_cast<const BoxInst&>(*prev);
                                if (prev_box.get_dest() == store.get_val()) {
                                    already_boxed = true;
                                }
                            }
                        }

                        if (!already_boxed) {
                            Operand boxed_temp = fn.create_temp(store.get_val().get_type());
                            auto box_inst = std::make_unique<BoxInst>(boxed_temp, store.get_val(), store.get_span());
                            store.get_val() = boxed_temp;
                            bb->insert_instruction(i, std::move(box_inst));
                            ++count;
                            ++i; // Advance past the original MemberStoreInst
                        }
                    }
                    break;
                }
                case Opcode::MemberLoad: {
                    if (!options_.auto_unbox_reads) break;
                    auto& load = static_cast<MemberLoadInst&>(*inst);
                    std::string cls_name = resolve_class_name(load.get_obj(), load.get_member(), fn);
                    if (!cls_name.empty() && is_recursive_field(cls_name, load.get_member())) {
                        // Check if next instruction is already UnboxInst for this dest
                        bool already_unboxed = false;
                        if (i + 1 < bb->get_instructions().size()) {
                            const auto& next = bb->get_instructions()[i + 1];
                            if (next && next->get_opcode() == Opcode::Unbox) {
                                const auto& next_unbox = static_cast<const UnboxInst&>(*next);
                                if (next_unbox.get_src() == load.get_dest()) {
                                    already_unboxed = true;
                                }
                            }
                        }

                        if (!already_unboxed) {
                            Operand orig_dest = load.get_dest();
                            Operand boxed_temp = fn.create_temp(orig_dest.get_type());
                            load.get_dest() = boxed_temp;
                            auto unbox_inst = std::make_unique<UnboxInst>(orig_dest, boxed_temp, load.get_span());
                            bb->insert_instruction(i + 1, std::move(unbox_inst));
                            ++count;
                            ++i; // Advance past the inserted UnboxInst
                        }
                    }
                    break;
                }
                case Opcode::ClassConstruct: {
                    if (!options_.auto_box_constructs) break;
                    auto& construct = static_cast<ClassConstructInst&>(*inst);
                    const std::string& cls_name = construct.get_class_name();
                    auto& fields = construct.get_fields();

                    for (auto& [field_name, field_val] : fields) {
                        if (is_recursive_field(cls_name, field_name)) {
                            // Check if already boxed
                            bool already_boxed = false;
                            if (i > 0) {
                                const auto& prev = bb->get_instructions()[i - 1];
                                if (prev && prev->get_opcode() == Opcode::Box) {
                                    const auto& prev_box = static_cast<const BoxInst&>(*prev);
                                    if (prev_box.get_dest() == field_val) {
                                        already_boxed = true;
                                    }
                                }
                            }

                            if (!already_boxed) {
                                Operand boxed_temp = fn.create_temp(field_val.get_type());
                                auto box_inst = std::make_unique<BoxInst>(boxed_temp, field_val, construct.get_span());
                                field_val = boxed_temp;
                                bb->insert_instruction(i, std::move(box_inst));
                                ++count;
                                ++i; // Advance past the inserted BoxInst
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return count;
}

size_t RecursiveBoxingPass::run_on_module(Module& mod) {
    size_t total = 0;
    for (const auto& fn : mod.get_functions()) {
        if (fn) {
            total += run_on_function(*fn);
        }
    }
    return total;
}

size_t RecursiveBoxingPass::run_on_module(Module& mod, const parser::ModuleAST& ast) {
    scan_classes(ast);
    return run_on_module(mod);
}

} // namespace keety::ir
