#include "tg_tree.hpp"
#include "utility.h"
namespace Zylann::TreeGen
{
    TG_Tree::TG_Tree()
    {
        _global_seed = 1337;
    }
    TG_Tree::~TG_Tree()
    {
    }

    static godot::Transform3D interpolate_path( const std::vector<godot::Transform3D> transforms, const std::vector<float> &distances, float offset )
    {

        // TG_CRASH_COND(transforms.size() == 0);
        // TG_CRASH_COND(transforms.size() != distances.size());

        if ( offset <= 0.f )
        {
            return transforms[0];
        }

        for ( size_t i = 0; i < distances.size(); ++i )
        {
            if ( distances[i] < offset )
            {
                continue;
            }
            // TG_CRASH_COND(i <= 0);
            const godot::Transform3D &prev_trans = transforms[i - 1];
            const godot::Transform3D &trans = transforms[i];
            const float prev_d = distances[i - 1];
            const float d = distances[i];
            const float t = ( offset - prev_d ) / ( d - prev_d );
            return prev_trans.interpolate_with( trans, t );
        }

        return transforms.back();
    }

    static float calc_length( const std::vector<godot::Transform3D> &transforms )
    {
        float length = 0.0f;
        for ( size_t i = 1; i < transforms.size(); ++i )
        {
            length += transforms[i - 1].origin.distance_to( transforms[i].origin );
        }
        return length;
    }

    static void scale_path( std::vector<godot::Transform3D> &transforms, const float scale )
    {
        for ( size_t i = 0; i < transforms.size(); ++i )
        {
            godot::Transform3D &t = transforms[i];
            t.origin = t.origin * scale;
        }
    }

    static void calc_orientations( std::vector<godot::Transform3D> &transforms, const float segment_length )
    {
        for ( size_t i = 0; i < transforms.size(); ++i )
        {
            godot::Transform3D &trans = transforms[i];

            godot::Transform3D prev_trans;
            if ( i > 0 )
            {
                prev_trans = transforms[i - 1];
            }
            else
            {
                prev_trans = trans;
                prev_trans.origin -= trans.basis.get_column( 1 ) * segment_length;
            }

            godot::Transform3D next_trans;
            if ( i + 1 < transforms.size() )
            {
                next_trans = transforms[i + 1];
            }
            else
            {
                next_trans = transforms[i];
                next_trans.origin += prev_trans.basis.get_column( 1 ) * segment_length;
            }

            const godot::Vector3 u0 = trans.origin - prev_trans.origin;
            const godot::Vector3 u1 = next_trans.origin - trans.origin;
            const godot::Vector3 u = ( u0 + u1 ).normalized();

            const godot::Vector3 a = u.cross( prev_trans.basis.get_column( 1 ) ).normalized();
            if ( a != godot::Vector3() )
            {
                trans.basis = prev_trans.basis.rotated( a, -prev_trans.basis.get_column( 1 ).angle_to( u ) );
            }
            else
            {
                trans.basis = prev_trans.basis;
            }
        }
    }

    inline void add_quad_indices( std::vector<int> &indices, int i0, int i1, int i2, int i3 )
    {
        //  --2---3--
        //    |  /|
        //    | / |
        //    |/  |
        //  --0---1--

        indices.push_back( i0 );
        indices.push_back( i3 );
        indices.push_back( i1 );

        indices.push_back( i0 );
        indices.push_back( i2 );
        indices.push_back( i3 );
    }

    static void connect_strips_with_same_point_count( std::vector<int> &indices, int prev_strip_begin, int point_count )
    {
        int i0 = prev_strip_begin;
        int i1 = prev_strip_begin + 1;
        int i2 = prev_strip_begin + point_count;
        int i3 = prev_strip_begin + point_count + 1;

        for ( int i = 1; i < point_count; ++i )
        {
            add_quad_indices( indices, i0, i1, i2, i3 );
            ++i0;
            ++i1;
            ++i2;
            ++i3;
        }
    }

    static void connect_strips_with_different_point_count( std::vector<int> &indices, int prev_strip_begin, int prev_point_count, int next_strip_begin, int next_point_count )
    {

        // Assumes strips have evenly sparsed points and their starting point is aligned.
        // If not then it would require an implementation that finds the closest points.

        // Could have symetrical functions... but harder to maintain

        bool flip_winding = false;
        if ( prev_point_count < next_point_count )
        {
            std::swap( prev_strip_begin, next_strip_begin );
            std::swap( prev_point_count, next_point_count );
            flip_winding = true;
        }

        // TG_CRASH_COND(prev_point_count <= next_point_count);

        const float k = static_cast<float>( next_point_count ) / static_cast<float>( prev_point_count );
        float c = 0.f;

        int src_i = prev_strip_begin;
        int dst_i = next_strip_begin;

        const size_t added_indices_begin = indices.size();

        //  o-------o-------o next
        //  |\     /|\     /|
        //  | \   / | \   / |
        //  |  \ /  |  \ /  |
        //  o---o---o---o---o prev

        for ( int i = 0; i < prev_point_count - 1; ++i )
        {
            const int prev_src_i = src_i;
            ++src_i;
            // Add lower triangle
            indices.push_back( prev_src_i );
            indices.push_back( dst_i );
            indices.push_back( src_i );
            c += k;
            if ( c >= 0.5f )
            {
                c -= 1.0f;
                const int prev_dst_i = dst_i;
                ++dst_i;
                // Add upper triangle
                indices.push_back( src_i );
                indices.push_back( prev_dst_i );
                indices.push_back( dst_i );
            }
        }

        if ( flip_winding )
        {
            for ( size_t i = added_indices_begin; i < indices.size(); i += 3 )
            {
                std::swap( indices[i], indices[i + 1] );
            }
        }
    }

    static TG_Tangents get_tangents_from_axes( const godot::Vector3 normal, godot::Vector3 binormal, const godot::Vector3 tangent )
    {

        float d = binormal.dot( normal.cross( tangent ) );
        TG_Tangents t;
        t.tangent = tangent;
        t.binormal_sign = d < 0.f ? -1.f : 1.f;
        return t;
    }

    static void add_cap( TG_SurfaceData &surface, const TG_SurfaceData &base_surface, const godot::Transform3D &trans, int point_count, bool flat, float radius )
    {

        // TG_CRASH_COND(point_count < 0);

        {
            // Duplicate vertices of the last ring
            const int i0 = static_cast<int>( base_surface.positions.size() ) - point_count;
            const godot::Transform3D trans_inv = trans.inverse();

            for ( int pi = 0; pi < point_count; ++pi )
            {
                const int i = i0 + pi;

                const godot::Vector3 p = base_surface.positions[i];
                surface.positions.push_back( p );

                if ( flat )
                {
                    surface.normals.push_back( trans.basis.get_column( 1 ) );

                    const TG_Tangents t = get_tangents_from_axes( trans.basis.get_column( 1 ), trans.basis.get_column( 0 ), trans.basis.get_column( 2 ) );
                    surface.tangents.push_back( t );

                    const godot::Vector3 local_pos = trans_inv.xform( p );
                    const godot::Vector2 uv( godot::Math::clamp( 0.5f + 0.5f * local_pos.x / radius, 0.f, 1.f ), godot::Math::clamp( 0.5f + 0.5f * local_pos.z / radius, 0.f, 1.f ) );
                    surface.uvs.push_back( uv );
                }
                else
                {
                    const godot::Vector3 n = base_surface.normals[i];
                    surface.normals.push_back( n );

                    const TG_Tangents t = base_surface.tangents[i];
                    surface.tangents.push_back( t );

                    const godot::Vector2 uv = base_surface.uvs[i];
                    surface.uvs.push_back( uv );
                }
            }
        }

        // Central vertex
        surface.positions.push_back( trans.origin );
        surface.normals.push_back( trans.basis.get_column( 1 ) );
        surface.uvs.push_back( godot::Vector2( 0.5f, 0.5f ) );
        {
            const TG_Tangents t = get_tangents_from_axes( trans.basis.get_column( 1 ), trans.basis.get_column( 0 ), trans.basis.get_column( 2 ) );
            surface.tangents.push_back( t );
        }

        // Create triangles

        int ib = static_cast<int>( surface.positions.size() ) - point_count - 1;
        int ie = static_cast<int>( surface.positions.size() ) - 1;

        int i0 = ib;

        for ( int i = 1; i < point_count; ++i )
        {
            surface.indices.push_back( i0 );
            ++i0;
            surface.indices.push_back( ie );
            surface.indices.push_back( i0 );
        }

        surface.indices.push_back( i0 );
        surface.indices.push_back( ie );
        surface.indices.push_back( ib );
    }

    static TG_SurfaceData &get_or_create_surface( std::vector<TG_SurfaceData> &surfaces, int surface_index )
    {
        if ( surface_index >= (int)surfaces.size() )
        {
            surfaces.resize( surface_index + 1 );
        }
        return surfaces[surface_index];
    }

    static void copy_range( TG_SurfaceData &surface, size_t src_i0, size_t count )
    {
        // TG_CRASH_COND(src_i0 >= surface.positions.size());

        const size_t dst_i0 = surface.positions.size();
        const size_t new_vertex_count = surface.positions.size() + count;

        surface.positions.resize( new_vertex_count );
        surface.normals.resize( new_vertex_count );
        surface.uvs.resize( new_vertex_count );
        surface.tangents.resize( new_vertex_count );

        for ( size_t i = 0; i < count; ++i )
        {
            const size_t src_i = src_i0 + i;
            const size_t dst_i = dst_i0 + i;

            surface.positions[dst_i] = surface.positions[src_i];
            surface.normals[dst_i] = surface.normals[src_i];
            surface.uvs[dst_i] = surface.uvs[src_i];
            surface.tangents[dst_i] = surface.tangents[src_i];
        }
    }

    static void generate_path_mesh( std::vector<TG_SurfaceData> &surfaces, const std::vector<godot::Transform3D> &transforms, const std::vector<float> &radii, const std::vector<float> distances, float mesh_divisions_per_unit, bool end_cap_flat, int main_material_index, int cap_material_index, godot::Vector2 uv_scale, bool constant_divisions )
    {

        // TG_CRASH_COND(transforms.size() != radii.size());
        ERR_FAIL_COND( main_material_index < 0 || main_material_index >= TG_Tree::MAX_MATERIALS );
        ERR_FAIL_COND( cap_material_index < 0 || cap_material_index >= TG_Tree::MAX_MATERIALS );

        int previous_ring_point_count = 0;

        if ( transforms.size() == 0 )
        {
            return;
        }

        // TODO Path shape
        // TODO Path welding

        // Main path
        {
            TG_SurfaceData &surface = get_or_create_surface( surfaces, main_material_index );

            const float base_circumference = static_cast<float>( Math_TAU ) * radii[0];
            const float uv_x_max = max( godot::Math::floor( base_circumference ), 1.f ) * uv_scale.x;

            const int base_point_count = max( static_cast<int>( base_circumference * mesh_divisions_per_unit ), 3 );

            for ( size_t transform_index = 0; transform_index < transforms.size(); ++transform_index )
            {
                const godot::Transform3D &trans = transforms[transform_index];
                const float radius = radii[transform_index];
                const float circumference = static_cast<float>( Math_TAU ) * radius;

                const int point_count = constant_divisions ? base_point_count : max( static_cast<int>( circumference * mesh_divisions_per_unit ), 3 );

                const float distance_along_path = distances[transform_index];

                // Make ring
                for ( int pi = 0; pi < point_count; ++pi )
                {
                    const float ak = static_cast<float>( pi ) / static_cast<float>( point_count );
                    const float angle = static_cast<float>( Math_TAU ) * ak;

                    const godot::Vector3 normal = trans.basis.get_column( 0 ).rotated( trans.basis.get_column( 1 ), angle );
                    const godot::Vector3 pos = trans.origin + radius * normal;

                    const godot::Vector2 uv( ak * uv_x_max, distance_along_path * uv_scale.y );

                    const TG_Tangents t = get_tangents_from_axes( normal, trans.basis.get_column( 1 ), trans.basis.get_column( 2 ).rotated( trans.basis.get_column( 1 ), angle ) );

                    surface.positions.push_back( pos );
                    surface.normals.push_back( normal );
                    surface.tangents.push_back( t );
                    surface.uvs.push_back( uv );
                }

                // Repeat last point, because it needs different UV.
                // That means topologically, we are making strips, not rings
                copy_range( surface, surface.positions.size() - point_count, 1 );
                surface.uvs.back().x = uv_x_max;

                if ( transform_index > 0 )
                {
                    // Connect to previous ring
                    if ( point_count == previous_ring_point_count )
                    {
                        const int prev_ring_begin = static_cast<int>( surface.positions.size() ) - 2 * ( point_count + 1 );
                        connect_strips_with_same_point_count( surface.indices, prev_ring_begin, point_count + 1 );
                    }
                    else
                    {
                        const int ring_begin = static_cast<int>( surface.positions.size() ) - point_count - 1;
                        const int prev_ring_begin = ring_begin - previous_ring_point_count - 1;
                        connect_strips_with_different_point_count( surface.indices, prev_ring_begin, previous_ring_point_count + 1, ring_begin, point_count + 1 );
                    }
                }

                previous_ring_point_count = point_count;
            }
        }

        // Cap
        {
            TG_SurfaceData &surface = get_or_create_surface( surfaces, cap_material_index );
            TG_SurfaceData &base_surface = get_or_create_surface( surfaces, main_material_index );
            add_cap( surface, base_surface, transforms.back(), previous_ring_point_count + 1, end_cap_flat, radii.back() );
        }
    }

    static void transform_positions( std::vector<godot::Vector3> &positions, size_t begin_index, size_t count, const godot::Transform3D &transform )
    {

        const size_t end = begin_index + count;
        for ( size_t i = begin_index; i < end; ++i )
        {
            positions[i] = transform.xform( positions[i] );
        }
    }

    static void transform_normals( std::vector<godot::Vector3> &normals, size_t begin_index, size_t count, const godot::Basis &basis )
    {

        const size_t end = begin_index + count;
        for ( size_t i = begin_index; i < end; ++i )
        {
            normals[i] = basis.xform( normals[i] );
        }
    }

    static void transform_tangents( std::vector<TG_Tangents> &tangents, size_t begin_index, size_t count, const godot::Basis &basis )
    {

        const size_t end = begin_index + count;
        for ( size_t i = begin_index; i < end; ++i )
        {
            tangents[i].tangent = basis.xform( tangents[i].tangent );
        }
    }

    static void offset_indices( std::vector<int> &indices, size_t begin_index, size_t count, int offset )
    {
        const size_t end = begin_index + count;
        for ( size_t i = begin_index; i < end; ++i )
        {
            indices[i] += offset;
        }
    }

    static void combine_mesh_surfaces( const TG_NodeInstance &node_instance, std::vector<TG_SurfaceData> &surfaces_per_material, const godot::Transform3D &parent_transform )
    {

        for ( size_t surface_index = 0; surface_index < node_instance.getSurfaces().size(); ++surface_index )
        {
            if ( surface_index >= surfaces_per_material.size() )
            {
                surfaces_per_material.resize( surface_index + 1 );
            }

            TG_SurfaceData &dst_surface = surfaces_per_material[surface_index];
            const TG_SurfaceData &src_surface = node_instance.getSurfaces()[surface_index];

            const godot::Transform3D transform = parent_transform * node_instance.getLocalTransform();

            const size_t vertices_begin_index = dst_surface.positions.size();
            const size_t vertices_count = src_surface.positions.size();

            const size_t indices_begin_index = dst_surface.indices.size();
            const size_t indices_count = src_surface.indices.size();

            raw_append_to( dst_surface.positions, src_surface.positions );
            raw_append_to( dst_surface.normals, src_surface.normals );
            raw_append_to( dst_surface.uvs, src_surface.uvs );
            raw_append_to( dst_surface.tangents, src_surface.tangents );
            raw_append_to( dst_surface.indices, src_surface.indices );

            transform_positions( dst_surface.positions, vertices_begin_index, vertices_count, transform );
            transform_normals( dst_surface.normals, vertices_begin_index, vertices_count, transform.basis );
            transform_tangents( dst_surface.tangents, vertices_begin_index, vertices_count, transform.basis );
            offset_indices( dst_surface.indices, indices_begin_index, indices_count, static_cast<int>( vertices_begin_index ) );

            for ( size_t i = 0; i < node_instance.getChildren().size(); ++i )
            {
                combine_mesh_surfaces( **node_instance.getChildren()[i], surfaces_per_material, transform );
            }
        }
    }

    static godot::Array combine_mesh_surfaces( const TG_NodeInstance &root_node_instance )
    {
        std::vector<TG_SurfaceData> surfaces;

        combine_mesh_surfaces( root_node_instance, surfaces, root_node_instance.getLocalTransform() );

        godot::Array gd_surfaces;
        gd_surfaces.resize( static_cast<int>( surfaces.size() ) );

        for ( size_t i = 0; i < surfaces.size(); ++i )
        {
            const TG_SurfaceData &src_surface = surfaces[i];
            godot::Array gd_surface;
            gd_surface.resize( godot::Mesh::ARRAY_MAX );

            gd_surface[godot::Mesh::ARRAY_VERTEX] = to_packed_array( src_surface.positions );
            gd_surface[godot::Mesh::ARRAY_NORMAL] = to_packed_array( src_surface.normals );
            gd_surface[godot::Mesh::ARRAY_TEX_UV] = to_packed_array( src_surface.uvs );
            //        gd_surface[godot::Mesh::ARRAY_TANGENT] = to_Packed_real_array_reinterpret(
            //        src_surface.tangents ); //TODO: I don't know how to solve this ~Syd
            gd_surface[godot::Mesh::ARRAY_INDEX] = to_packed_array( src_surface.indices );
            gd_surfaces[static_cast<int>( i )] = gd_surface;
        }

        return gd_surfaces;
    }

    static void generate_node_leaf( const TG_Node &node, TG_NodeInstance &node_instance, godot::RandomNumberGenerator &rng )
    {
        const TG_LeafParams &leaf_params = node.get_leaf_params();

        auto surfaces = node_instance.getSurfaces();
        TG_SurfaceData &surface = get_or_create_surface( surfaces , leaf_params.getMaterialIndex() );
        const godot::Transform3D trans = godot::Transform3D(); // node_instance.local_transform;

        float scale = leaf_params.getScale();
        if ( leaf_params.getScaleJitter() > 0.f )
        {
            scale += leaf_params.getScaleJitter() * rng.randf_range( -leaf_params.getScale(), leaf_params.getScale() );
        }

        // Let's start with a simple quad
        // TODO Configurable params
        const float width = 1.f * scale;
        const float height = 1.f * scale;

        //  3-------2
        //  |     - |
        //  |   -   |    y
        //  | -     |    |
        //  0---o---1    o---x

        // TODO Y rotation

        const godot::Vector3 xv = trans.basis.get_column( 0 ) * width * 0.5f;

        surface.positions.push_back( trans.origin - xv );
        surface.positions.push_back( trans.origin + xv );
        surface.positions.push_back( trans.origin + xv + trans.basis.get_column( 1 ) * height );
        surface.positions.push_back( trans.origin - xv + trans.basis.get_column( 1 ) * height );

        surface.normals.push_back( trans.basis.get_column( 2 ) );
        surface.normals.push_back( trans.basis.get_column( 2 ) );
        surface.normals.push_back( trans.basis.get_column( 2 ) );
        surface.normals.push_back( trans.basis.get_column( 2 ) );

        // TODO Use an atlas?
        surface.uvs.emplace_back( 0.f, 1.f );
        surface.uvs.emplace_back( 1.f, 1.f );
        surface.uvs.emplace_back( 1.f, 0.f );
        surface.uvs.emplace_back( 0.f, 0.f );

        const TG_Tangents tangents = get_tangents_from_axes( trans.basis.get_column( 2 ), trans.basis.get_column( 1 ), trans.basis.get_column( 0 ) );
        surface.tangents.push_back( tangents );
        surface.tangents.push_back( tangents );
        surface.tangents.push_back( tangents );
        surface.tangents.push_back( tangents );

        add_quad_indices( surface.indices, 0, 1, 3, 2 );
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    int TG_Tree::getGlobalSeed() const
    {
        return _global_seed;
    }

    void TG_Tree::setGlobalSeed( int p_seed )
    {
        _global_seed = p_seed;
    }

    float TG_Tree::get_mesh_divisions_per_unit() const
    {
        return _mesh_divisions_per_unit;
    }

    void TG_Tree::set_mesh_divisions_per_unit( float d )
    {
        _mesh_divisions_per_unit = d;
    }

    float TG_Tree::get_branch_segments_per_unit() const
    {
        return _branch_segments_per_unit;
    }

    void TG_Tree::set_branch_segments_per_unit( float b )
    {
        _branch_segments_per_unit = b;
    }

    bool TG_Tree::get_constant_mesh_divisions() const
    {
        return _constant_mesh_divisions;
    }

    void TG_Tree::set_constant_mesh_divisions( bool b )
    {
        _constant_mesh_divisions = b;
    }

    godot::Ref<TG_Node> TG_Tree::get_root_node() const
    {
        return _root_node;
    }

    void TG_Tree::set_root_node( godot::Ref<TG_Node> node )
    {
        _root_node = node;
    }

    godot::Ref<TG_NodeInstance> TG_Tree::get_root_node_instance() const
    {
        return _root_instance;
    }

    godot::Array TG_Tree::generate()
    {
        ERR_FAIL_COND_V( _root_node.is_null(), godot::Array() );
        _root_instance.instantiate();
        if ( !_root_node->is_active() )
        {
            return godot::Array();
        }
        godot::Ref<godot::RandomNumberGenerator> rng;
        rng.instantiate();
        rng->set_seed( _global_seed + _root_node->get_local_seed() );
        process_node( **_root_node, **_root_instance, godot::Vector3( 0, 1, 0 ), **rng );
        godot::Array surfaces = combine_mesh_surfaces( **_root_instance );
        return surfaces;
    }

    void TG_Tree::process_node( const TG_Node &node, TG_NodeInstance &node_instance, godot::Vector3 sun_dir_local, godot::RandomNumberGenerator &rng )
    {

        switch ( node.get_type() )
        {
            case TG_Node::TYPE_BRANCH:
                generate_node_path( node, node_instance, sun_dir_local, rng );
                break;

            case TG_Node::TYPE_LEAF:
                generate_node_leaf( node, node_instance, rng );
                break;

            default:
                godot::Array args;
                args.append( static_cast<int>( node.get_type() ) );
                ERR_PRINT( godot::String( "Unknown node type {0}" ).format( args ) );
                break;
        }

        if ( node.get_child_count() == 0 )
        {
            return;
        }

        const float path_length = node_instance.getPathDistances().back();
        std::vector<SpawnInfo> spawns;

        // Process children
        for ( int i = 0; i < node.get_child_count(); ++i )
        {
            const TG_Node &child = node.get_child_internal( i );

            if ( !child.is_active() )
            {
                continue;
            }

            godot::Ref<godot::RandomNumberGenerator> child_rng;
            child_rng.instantiate();
            child_rng->set_seed( _global_seed + child.get_local_seed() );

            generate_spawns( spawns, child.get_spawn_params(), **child_rng, path_length );

            for ( size_t j = 0; j < spawns.size(); ++j )
            {
                const SpawnInfo &spawn_info = spawns[j];
                const float offset = spawn_info.offset_ratio * path_length;
                // TODO Sample parent radius and offset child accordingly
                const godot::Transform3D path_trans = interpolate_path( node_instance.getPath(), node_instance.getPathDistances(), offset );

                godot::Ref<TG_NodeInstance> child_node_instance;
                child_node_instance.instantiate();
                child_node_instance->setOffsetRatio(spawn_info.offset_ratio);
                child_node_instance->setLocalTransform(godot::Transform3D( path_trans.basis * spawn_info.basis, path_trans.origin ));

                process_node( child, **child_node_instance, child_node_instance->getLocalTransform().basis.inverse().xform( sun_dir_local ), **child_rng );

                node_instance.addChild( child_node_instance );
            }
        }
    }

    void TG_Tree::generate_node_path( const TG_Node &node, TG_NodeInstance &node_instance, godot::Vector3 sun_dir_local, godot::RandomNumberGenerator &rng )
    {

        const TG_SpawnParams &spawn_params = node.get_spawn_params();

        const float relative_offset_ratio = ( node_instance.getOffsetRatio() - spawn_params.getAlongBeginRatio() ) / ( spawn_params.getAlongEndRatio() - spawn_params.getAlongBeginRatio() );

        const TG_PathParams &path_params = node.get_path_params();

        // Calculate expected length

        float length_with_modifiers = path_params.getLength();
        if ( path_params.getLengthCurveAlongParent().is_valid() )
        {
            const godot::Ref<godot::Curve> &curve = path_params.getLengthCurveAlongParent();
            length_with_modifiers *= curve->sample_baked( relative_offset_ratio );
        }
        length_with_modifiers += path_params.getLengthRandomness() * rng.randf_range( -1.0, 1.0 ) * length_with_modifiers;

        //    float radius_multiplier = 1.0;
        //    if ( path_params.radius_curve_along_parent.is_valid() )
        //    {
        //        godot::Ref<godot::Curve> curve = path_params.radius_curve_along_parent;
        //        radius_multiplier *= curve->sample_baked( relative_offset_ratio );
        //    }

        const int point_count = max( static_cast<int>( _branch_segments_per_unit * length_with_modifiers ), 2 );
        const float segment_length = 1.0f / _branch_segments_per_unit;

        godot::Basis sun_basis;

        std::vector<Transform3D> path = node_instance.getPath();
        std::vector<float> radii = node_instance.getPathRadii();
        path.clear();
        radii.clear();

        const float distance_step = length_with_modifiers / point_count;

        // Plot base points

        godot::Transform3D trans;
        const float default_radius = 0.5f * ( path_params.getMinRadius() + path_params.getMaxRadius() );
        godot::Ref<godot::Curve> radius_curve = path_params.getRadiusCurve();

        for ( int i = 0; i < point_count; ++i )
        {
            const float k = static_cast<float>( i ) / point_count;

            const float r = path_params.getRadiusCurve().is_valid() ? godot::Math::lerp( (double)path_params.getMinRadius(), (double)path_params.getMaxRadius(), radius_curve->sample_baked( k ) ) : default_radius;

            radii.push_back( r );
            path.push_back( trans );

            if ( path_params.getSeekSun() != 0.f )
            {
                const float seek_sun = path_params.getSeekSun() / _branch_segments_per_unit;
                const godot::Vector3 tend_dir = sun_dir_local * godot::Math::sign( seek_sun );
                const float a = godot::Math::sign( seek_sun ) * trans.basis.get_column( 1 ).angle_to( tend_dir );
                if ( fabs( a ) > 0.001f )
                {
                    godot::Vector3 axis = trans.basis.get_column( 1 ).cross( tend_dir ).normalized();
                    trans.basis = trans.basis.rotated( axis, a * seek_sun );
                }
            }

            trans.origin += distance_step * trans.basis.get_column( 1 );
        }

        // Apply noise
        if ( path_params.getNoiseAmplitude() != 0.f )
        {
            // TODO Add `noise_discrepancy` to make each branch bifurcate more diferently
            godot::Ref<godot::FastNoiseLite> noise_x;
            godot::Ref<godot::FastNoiseLite> noise_y;
            godot::Ref<godot::FastNoiseLite> noise_z;

            noise_x.instantiate();
            noise_x->set_seed( _global_seed + node.get_local_seed() );
            noise_x->set_fractal_octaves( path_params.getNoiseOctaves() );
            //        noise_x->set_period( path_params.noise_period ); //TODO. what is the alternative in gd4 to this ~Syd

            noise_y.instantiate();
            noise_y->set_seed( _global_seed + node.get_local_seed() + 1 );
            noise_y->set_fractal_octaves( path_params.getNoiseOctaves() );
            //        noise_y->set_period( path_params.noise_period ); //TODO. what is the alternative in gd4 to this ~Syd

            noise_z.instantiate();
            noise_z->set_seed( _global_seed + node.get_local_seed() + 2 );
            noise_z->set_fractal_octaves( path_params.getNoiseOctaves() );
            //        noise_z->set_period( path_params.noise_period ); //TODO. what is the alternative in gd4 to this ~Syd

            for ( int i = 0; i < point_count; ++i )
            {
                const float k = static_cast<float>( i ) / point_count;
                godot::Transform3D &transPath = path[i]; // TODO: was godot::Transform3D &trans = path[i];
                const float amp = path_params.getNoiseAmplitude() * pow( k, path_params.getNoiseCurve() );
                const godot::Vector3 disp = amp * godot::Vector3( noise_x->get_noise_3dv( transPath.origin ), noise_y->get_noise_3dv( transPath.origin ), noise_z->get_noise_3dv( transPath.origin ) );
                transPath.origin += disp;
            }
        }

        // Renormalize length
        const float length = calc_length( path );
        if ( length > 0.0f )
        {
            const float rscale = length_with_modifiers / length;
            scale_path( path, rscale );
        }

        // TODO Optimize path so straight parts have less points

        // Bake distances
        std::vector<float> distances = node_instance.getPathDistances();
        distances.clear();
        distances.push_back( 0 );
        for ( size_t i = 1; i < path.size(); ++i )
        {
            distances.push_back( distances[i - 1] + path[i].origin.distance_to( path[i - 1].origin ) );
        }

        // Recalculate orientations after modifiers
        calc_orientations( path, segment_length );

        auto surfaces = node_instance.getSurfaces();
        generate_path_mesh( surfaces, path, radii, distances, _mesh_divisions_per_unit, path_params.isEndCapFlat(), path_params.getMainMaterialIndex(), path_params.getCapMaterialIndex(), path_params.getUvScale(), _constant_mesh_divisions );
    }

    void TG_Tree::generate_spawns( std::vector<SpawnInfo> &transforms, const TG_SpawnParams &params, godot::RandomNumberGenerator &rng, float parent_length )
    {

        transforms.clear();

        const int amount = params.getAlongBaseAmount() + static_cast<int>( params.getAlongAmountPerUnit() * parent_length );

        if ( amount == 0 )
        {
            return;
        }

        const float k_along_jitter = 0.5f * params.getAlongJitter() * ( params.getAlongEndRatio() - params.getAlongBeginRatio() ) / static_cast<float>( amount );

        const float a_jitter = 0.5f * params.getAroundJitter() / static_cast<float>( params.getAroundAmount() );
        const float half_pi = static_cast<float>( Math_PI ) * 0.5f;

        for ( int i = 0; i < amount; ++i )
        {
            float k = godot::Math::lerp( params.getAlongBeginRatio(), params.getAlongEndRatio(), static_cast<float>( i ) / static_cast<float>( amount ) );

            k += rng.randf_range( -k_along_jitter, k_along_jitter );

            for ( int j = 0; j < params.getAroundAmount(); ++j )
            {
                if ( rng.randf() < params.getSkipProbability() )
                {
                    continue;
                }

                const float v_angle = params.getVerticalAngle() + params.getVerticalAngleJitter() * rng.randf_range( -half_pi, half_pi );

                godot::Basis basis = godot::Basis().rotated( godot::Vector3( 1, 0, 0 ), v_angle );

                float af = static_cast<float>( j ) / static_cast<float>( params.getAroundAmount() );
                af += rng.randf_range( -a_jitter, a_jitter );

                basis = basis.rotated( godot::Vector3( 0, 1, 0 ), af * static_cast<float>( Math_TAU ) + params.getAroundOffset() );

                SpawnInfo si;
                si.basis = basis;
                si.offset_ratio = k;
                transforms.push_back( si );
            }
        }
    }

    void TG_Tree::_bind_methods()
    {
        godot::ClassDB::bind_method( godot::D_METHOD( "setGlobalSeed" ), &TG_Tree::setGlobalSeed );
        godot::ClassDB::bind_method( godot::D_METHOD( "getGlobalSeed" ), &TG_Tree::getGlobalSeed );
        ADD_PROPERTY( PropertyInfo( Variant::FLOAT, "global_seed" ), "setGlobalSeed", "getGlobalSeed" );

        godot::ClassDB::bind_method( godot::D_METHOD( "get_mesh_divisions_per_unit" ), &TG_Tree::get_mesh_divisions_per_unit );
        godot::ClassDB::bind_method( godot::D_METHOD( "set_mesh_divisions_per_unit" ), &TG_Tree::set_mesh_divisions_per_unit );

        godot::ClassDB::bind_method( godot::D_METHOD( "get_branch_segments_per_unit" ), &TG_Tree::get_branch_segments_per_unit );
        godot::ClassDB::bind_method( godot::D_METHOD( "set_branch_segments_per_unit" ), &TG_Tree::set_branch_segments_per_unit );

        godot::ClassDB::bind_method( godot::D_METHOD( "get_constant_mesh_divisions" ), &TG_Tree::get_constant_mesh_divisions );
        godot::ClassDB::bind_method( godot::D_METHOD( "set_constant_mesh_divisions" ), &TG_Tree::set_constant_mesh_divisions );

        godot::ClassDB::bind_method( godot::D_METHOD( "get_root_node" ), &TG_Tree::get_root_node );
        godot::ClassDB::bind_method( godot::D_METHOD( "set_root_node" ), &TG_Tree::set_root_node );

        godot::ClassDB::bind_method( godot::D_METHOD( "get_root_node_instance" ), &TG_Tree::get_root_node_instance );

        godot::ClassDB::bind_method( godot::D_METHOD( "generate" ), &TG_Tree::generate );
    }
}