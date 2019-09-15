//=============================================================================

#include <pmp/algorithms/SurfaceTriangulation.h>

//=============================================================================

namespace pmp {

//=============================================================================

SurfaceTriangulation::SurfaceTriangulation(SurfaceMesh& _mesh) : mesh_(_mesh)
{
    points_ = mesh_.vertex_property<Point>("v:point");
}

//-----------------------------------------------------------------------------

void SurfaceTriangulation::triangulate()
{
    for (auto f: mesh_.faces())
        triangulate(f);

    mesh_.garbage_collection();

    std::cout << "Triangle mesh? " << mesh_.is_triangle_mesh() << std::endl;
}

//-----------------------------------------------------------------------------

void SurfaceTriangulation::triangulate(Face f)
{
    Halfedge _h = mesh_.halfedge(f);

    // collect polygon halfedges
    halfedges_.clear();
    vertices_.clear();
    Halfedge h = _h;
    do
    {
        if (!mesh_.is_manifold(mesh_.to_vertex(h)))
        {
            std::cerr << "[SurfaceTriangulation] Non-manifold polygon\n";
            return;
        }

        halfedges_.push_back(h);
        vertices_.push_back(mesh_.to_vertex(h));
    } 
    while ((h = mesh_.next_halfedge(h)) != _h);

    // do we have at least four vertices?
    const int n = halfedges_.size();
    if (n <= 3) return;

    // delete polygon
    mesh_.delete_face(f);

    // compute minimal triangulation by dynamic programming
    weight_.clear();
    weight_.resize(n, std::vector<Scalar>(n, FLT_MAX));
    index_.clear();
    index_.resize(n, std::vector<int>(n, 0));

    int i, j, m, k, imin;
    Scalar w, wmin;

    // initialize 2-gons
    for (i = 0; i < n - 1; ++i)
    {
        weight_[i][i + 1] = 0.0;
        index_[i][i + 1] = -1;
    }

    // n-gons with n>2
    for (j = 2; j < n; ++j)
    {
        // for all n-gons [i,i+j]
        for (i = 0; i < n - j; ++i)
        {
            k = i + j;
            wmin = FLT_MAX;
            imin = -1;

            // find best split i < m < i+j
            for (m = i + 1; m < k; ++m)
            {
                w = weight_[i][m] + compute_weight(i, m, k) + weight_[m][k];
                if (w < wmin)
                {
                    wmin = w;
                    imin = m;
                }
            }

            weight_[i][k] = wmin;
            index_[i][k] = imin;
        }
    }

    // now add triangles to mesh
    std::vector<ivec2> todo;
    todo.reserve(n);
    todo.push_back(ivec2(0, n - 1));
    while (!todo.empty())
    {
        ivec2 tri = todo.back();
        todo.pop_back();
        int start = tri[0];
        int end = tri[1];
        if (end - start < 2)
            continue;
        int split = index_[start][end];

        //insert_edge(start, split);
        //insert_edge(split, end);
        mesh_.add_triangle(vertices_[start], vertices_[split], vertices_[end]);
        
        todo.push_back(ivec2(start, split));
        todo.push_back(ivec2(split, end));
    }


    // clean up
    weight_.clear();
    index_.clear();
    halfedges_.clear();
    vertices_.clear();
}

//-----------------------------------------------------------------------------

Scalar SurfaceTriangulation::compute_weight(int i, int j, int k) const
{
    const Vertex a = vertices_[i];
    const Vertex b = vertices_[j];
    const Vertex c = vertices_[k];

    // if one of the potential edges already exists as NON-boundary edge
    // this would result in an invalid triangulation
    // -> prevent by giving infinite weight
    // (this happens for suzanne.obj!)
    if (is_interior_edge(a, b) || 
        is_interior_edge(b, c) ||
        is_interior_edge(c, a))
        return FLT_MAX;
    //if (is_edge(a,b) && is_edge(b,c) && is_edge(c,a))
        //return FLT_MAX;

    // compute area
    return sqrnorm(cross(points_[b] - points_[a], points_[c] - points_[a]));
}

//-----------------------------------------------------------------------------

bool SurfaceTriangulation::is_edge(Vertex _a, Vertex _b) const
{
    return mesh_.find_halfedge(_a, _b).is_valid();
}

//-----------------------------------------------------------------------------

bool SurfaceTriangulation::is_interior_edge(Vertex _a, Vertex _b) const
{
    Halfedge h = mesh_.find_halfedge(_a, _b);
    if (!h.is_valid())
        return false; // edge does not exist
    return (!mesh_.is_boundary(h) &&
            !mesh_.is_boundary(mesh_.opposite_halfedge(h)));
}

//-----------------------------------------------------------------------------

bool SurfaceTriangulation::insert_edge(int i, int j)
{
    Halfedge h0 = halfedges_[i];
    Halfedge h1 = halfedges_[j];
    Vertex   v0 = vertices_[i];
    Vertex   v1 = vertices_[j];

    // does edge already exist?
    if (mesh_.find_halfedge(v0, v1).is_valid())
    {
        return false;
    }

    // can we reach v1 from h0?
    {
        Halfedge h = h0;
        do {
            h = mesh_.next_halfedge(h);
            if (mesh_.to_vertex(h) == v1)
            {
                mesh_.insert_edge(h0, h);
                return true;
            }
        } while (h != h0);
    }

    // can we reach v0 from h1?
    {
        Halfedge h = h1;
        do {
            h = mesh_.next_halfedge(h);
            if (mesh_.to_vertex(h) == v0)
            {
                mesh_.insert_edge(h1, h);
                return true;
            }
        } while (h != h1);
    }

    std::cerr << "[SurfaceTriangulation] This should not happen...\n"; 
    return false;
}

//=============================================================================
}
//=============================================================================