#include "tg_node.hpp"

TG_Node::TG_Node()
{
    _spawn_params.instantiate();
    _path_params.instantiate();
    _leaf_params.instantiate();
}

TG_Node::~TG_Node()
{
}

TG_Node::Type TG_Node::get_type() const
{
    return _type;
}
void TG_Node::set_type( TG_Node::Type type )
{
    _type = type;
}
int TG_Node::get_local_seed() const
{
    return _local_seed;
}
void TG_Node::set_local_seed( int p_local_seed )
{
    _local_seed = p_local_seed;
}
bool TG_Node::is_active() const
{
    return _active;
}
void TG_Node::set_active( bool active )
{
    _active = active;
}
int TG_Node::get_child_count() const
{
    return static_cast<int>( _children.size() );
}
void TG_Node::add_child( godot::Ref<TG_Node> node )
{
    _children.push_back( node );
}
void TG_Node::clear_children()
{
    _children.clear();
}
const TG_SpawnParams &TG_Node::get_spawn_params() const
{
    // TG_CRASH_COND(_spawn_params.is_null());
    return **_spawn_params;
}
const TG_PathParams &TG_Node::get_path_params() const
{
    // TG_CRASH_COND(_path_params.is_null());
    return **_path_params;
}
const TG_LeafParams &TG_Node::get_leaf_params() const
{
    // TG_CRASH_COND(_leaf_params.is_null());
    return **_leaf_params;
}
const TG_Node &TG_Node::get_child_internal( int i ) const
{
    const godot::Ref<TG_Node> &ref = _children[i];
    // TG_CRASH_COND(ref.is_null());
    return **ref;
}
godot::Ref<TG_SpawnParams> TG_Node::_b_get_spawn_params()
{
    return _spawn_params;
}
godot::Ref<TG_PathParams> TG_Node::_b_get_path_params()
{
    return _path_params;
}
godot::Ref<TG_LeafParams> TG_Node::_b_get_leaf_params()
{
    return _leaf_params;
}
godot::Ref<TG_Node> TG_Node::_b_get_child( int i )
{
    // ERR_FAIL_INDEX_V(i, _children.size(), godot::Ref<TG_Node>());
    return _children[i];
}
void TG_Node::_b_set_type( int type )
{
    ERR_FAIL_INDEX( type, TYPE_COUNT );
    _type = static_cast<Type>( type );
}
void TG_Node::_bind_methods()
{
    godot::ClassDB::bind_method( godot::D_METHOD( "get_spawn_params" ), &TG_Node::_b_get_spawn_params );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_path_params" ), &TG_Node::_b_get_path_params );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_leaf_params" ), &TG_Node::_b_get_leaf_params );

    godot::ClassDB::bind_method( godot::D_METHOD( "get_local_seed" ), &TG_Node::get_local_seed );
    godot::ClassDB::bind_method( godot::D_METHOD( "set_local_seed" ), &TG_Node::set_local_seed );

    godot::ClassDB::bind_method( godot::D_METHOD( "is_active" ), &TG_Node::is_active );
    godot::ClassDB::bind_method( godot::D_METHOD( "set_active" ), &TG_Node::set_active );

    godot::ClassDB::bind_method( godot::D_METHOD( "set_type" ), &TG_Node::_b_set_type );

    godot::ClassDB::bind_method( godot::D_METHOD( "get_child_count" ), &TG_Node::get_child_count );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_child" ), &TG_Node::_b_get_child );
    godot::ClassDB::bind_method( godot::D_METHOD( "add_child" ), &TG_Node::add_child );
    godot::ClassDB::bind_method( godot::D_METHOD( "clear_children" ), &TG_Node::clear_children );

    /*godot::register_method("get_spawn_params", &TG_Node::_b_get_spawn_params);
    godot::register_method("get_path_params", &TG_Node::_b_get_path_params);
    godot::register_method("get_leaf_params", &TG_Node::_b_get_leaf_params);

    godot::register_method("get_local_seed", &TG_Node::get_local_seed);
    godot::register_method("set_local_seed", &TG_Node::set_local_seed);

    godot::register_method("is_active", &TG_Node::is_active);
    godot::register_method("set_active", &TG_Node::set_active);

    godot::register_method("set_type", &TG_Node::_b_set_type);

    godot::register_method("get_child_count", &TG_Node::get_child_count);
    godot::register_method("get_child", &TG_Node::get_child);
    godot::register_method("add_child", &TG_Node::add_child);
    godot::register_method("clear_children", &TG_Node::clear_children);*/
}