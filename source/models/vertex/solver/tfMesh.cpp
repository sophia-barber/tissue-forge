/*******************************************************************************
 * This file is part of Tissue Forge.
 * Copyright (c) 2022 T.J. Sego and Tien Comlekoglu
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 ******************************************************************************/

#include "tfMesh.h"

#include "tfMeshSolver.h"

#include <tfEngine.h>
#include <tfLogger.h>

#include <algorithm>


#define TF_MESH_GETPART(idx, inv) idx >= inv.size() ? NULL : inv[idx]


using namespace TissueForge;
using namespace TissueForge::models::vertex;


HRESULT Mesh_checkUnstoredObj(MeshObj *obj) {
    if(!obj || obj->objId >= 0 || obj->mesh) {
        TF_Log(LOG_ERROR);
        return E_FAIL;
    }
    
    return S_OK;
}


HRESULT Mesh_checkStoredObj(MeshObj *obj, Mesh *mesh) {
    if(!obj || obj->objId < 0 || obj->mesh == NULL || obj->mesh != mesh) {
        TF_Log(LOG_ERROR);
        return E_FAIL;
    }

    return S_OK;
}


template <typename T> 
HRESULT Mesh_checkObjStorage(MeshObj *obj, std::vector<T*> inv) {
    if(!Mesh_checkStoredObj(obj) || obj->objId >= inv.size()) {
        TF_Log(LOG_ERROR);
        return E_FAIL;
    }

    return S_OK;
}


template <typename T> 
int Mesh_invIdAndAlloc(std::vector<T*> &inv, std::set<unsigned int> &availIds, T *obj=NULL) {
    if(!availIds.empty()) {
        std::set<unsigned int>::iterator itr = availIds.begin();
        unsigned int objId = *itr;
        if(obj) {
            inv[objId] = obj;
            obj->objId = *itr;
        }
        availIds.erase(itr);
        return objId;
    }
    
    int res = inv.size();
    int new_size = inv.size() + TFMESHINV_INCR;
    inv.reserve(new_size);
    for(unsigned int i = res; i < new_size; i++) {
        inv.push_back(NULL);
        if(i != res) 
            availIds.insert(i);
    }
    if(obj) {
        inv[res] = obj;
        obj->objId = res;
    }
    return res;
}


#define TF_MESH_OBJINVCHECK(obj, inv) \
    { \
        if(obj->objId >= inv.size()) { \
            TF_Log(LOG_ERROR) << "Object with id " << obj->objId << " exceeds inventory (" << inv.size() << ")"; \
            return E_FAIL; \
        } \
    }


template <typename T> 
HRESULT Mesh_addObj(Mesh *mesh, std::vector<T*> &inv, std::set<unsigned int> &availIds, T *obj, MeshSolver *_solver=NULL) {
    if(Mesh_checkUnstoredObj(obj) != S_OK || !obj->validate()) {
        TF_Log(LOG_ERROR);
        return E_FAIL;
    }

    obj->objId = Mesh_invIdAndAlloc(inv, availIds, obj);
    obj->mesh = mesh;
    if(_solver) {
        std::vector<int> objIds{obj->objId};
        std::vector<MeshObj::Type> objTypes{obj->objType()};
        for(auto &p : obj->parents()) {
            objIds.push_back(p->objId);
            objTypes.push_back(p->objType());
        }
        _solver->log(mesh, MeshLogEventType::Create, objIds, objTypes);
    }
    return S_OK;
}

HRESULT Mesh::add(Vertex *obj) { 
    isDirty = true;
    if(_solver) 
        _solver->setDirty(true);

    return Mesh_addObj(this, this->vertices, this->vertexIdsAvail, obj, _solver);
}

HRESULT Mesh::add(Surface *obj){ 
    isDirty = true;

    for(auto &v : obj->vertices) 
        if(v->objId < 0 && add(v) != S_OK) {
            TF_Log(LOG_ERROR);
            return E_FAIL;
        }

    if(Mesh_addObj(this, this->surfaces, this->surfaceIdsAvail, obj, _solver) != S_OK) {
        TF_Log(LOG_ERROR);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Mesh::add(Body *obj){ 
    isDirty = true;

    for(auto &s : obj->surfaces) 
        if(s->objId < 0 && add((Surface*)s) != S_OK) {
            TF_Log(LOG_ERROR);
            return E_FAIL;
        }

    if(Mesh_addObj(this, this->bodies, this->bodyIdsAvail, obj, _solver) != S_OK) {
        TF_Log(LOG_ERROR);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Mesh::add(Structure *obj){ 
    isDirty = true;

    for(auto &p : obj->parents()) {
        if(p->objId >= 0) 
            continue;

        if(TissueForge::models::vertex::check(p, MeshObj::Type::STRUCTURE)) {
            if(add((Structure*)p) != S_OK) {
                TF_Log(LOG_ERROR);
                return E_FAIL;
            }
        } 
        else if(TissueForge::models::vertex::check(p, MeshObj::Type::BODY)) {
            if(add((Body*)p) != S_OK) {
                TF_Log(LOG_ERROR);
                return E_FAIL;
            }
        }
        else {
            TF_Log(LOG_ERROR) << "Could not determine type of parent";
            return E_FAIL;
        }
    }

    if(Mesh_addObj(this, this->structures, this->structureIdsAvail, obj, _solver) != S_OK) {
        TF_Log(LOG_ERROR);
        return E_FAIL;
    }

    return S_OK;
}


HRESULT Mesh::removeObj(MeshObj *obj) { 
    isDirty = true;

    if(Mesh_checkStoredObj(obj, this) != S_OK) {
        TF_Log(LOG_ERROR) << "Invalid mesh object passed for remove.";
        return E_FAIL;
    } 

    if(TissueForge::models::vertex::check(obj, MeshObj::Type::VERTEX)) {
        TF_MESH_OBJINVCHECK(obj, this->vertices);
        this->vertices[obj->objId] = NULL;
        this->vertexIdsAvail.insert(obj->objId);
    } 
    else if(TissueForge::models::vertex::check(obj, MeshObj::Type::SURFACE)) {
        TF_MESH_OBJINVCHECK(obj, this->surfaces);
        this->surfaces[obj->objId] = NULL;
        this->surfaceIdsAvail.insert(obj->objId);
    } 
    else if(TissueForge::models::vertex::check(obj, MeshObj::Type::BODY)) {
        TF_MESH_OBJINVCHECK(obj, this->bodies);
        this->bodies[obj->objId] = NULL;
        this->bodyIdsAvail.insert(obj->objId);
    } 
    else if(TissueForge::models::vertex::check(obj, MeshObj::Type::STRUCTURE)) {
        TF_MESH_OBJINVCHECK(obj, this->structures);
        this->structures[obj->objId] = NULL;
        this->structureIdsAvail.insert(obj->objId);
    } 
    else {
        TF_Log(LOG_ERROR) << "Mesh object type could not be determined.";
        return E_FAIL;
    }

    if(_solver)
        _solver->log(this, MeshLogEventType::Destroy, {obj->objId}, {obj->objType()});
    obj->objId = -1;
    obj->mesh = NULL;

    for(auto &c : obj->children()) 
        if(removeObj(c) != S_OK) {
            TF_Log(LOG_ERROR);
            return E_FAIL;
        }

    return S_OK;
}

Vertex *Mesh::findVertex(const FVector3 &pos, const float &tol) {

    for(auto &v : vertices)
        if(v->particle()->relativePosition(pos).length() <= tol) 
            return v;

    return NULL;
}

Vertex *Mesh::getVertex(const unsigned int &idx) {
    return TF_MESH_GETPART(idx, vertices);
}

Surface *Mesh::getSurface(const unsigned int &idx) {
    return TF_MESH_GETPART(idx, surfaces);
}

Body *Mesh::getBody(const unsigned int &idx) {
    return TF_MESH_GETPART(idx, bodies);
}

Structure *Mesh::getStructure(const unsigned int &idx) {
    return TF_MESH_GETPART(idx, structures);
}

template <typename T> 
bool Mesh_validateInv(const std::vector<T*> &inv) {
    for(auto &o : inv) 
        if(!o->validate()) 
            return false;
    return true;
}

bool Mesh::validate() {
    if(!Mesh_validateInv(this->vertices)) 
        return false;
    else if(!Mesh_validateInv(this->surfaces)) 
        return false;
    else if(!Mesh_validateInv(this->bodies)) 
        return false;
    else if(!Mesh_validateInv(this->structures)) 
        return false;
    return true;
}

HRESULT Mesh::makeDirty() {
    isDirty = true;
    if(_solver) 
        if(_solver->setDirty(true) != S_OK) 
            return E_FAIL;
    return S_OK;
}

bool Mesh::connected(Vertex *v1, Vertex *v2) {
    for(auto &s1 : v1->surfaces) 
        for(auto vitr = s1->vertices.begin() + 1; vitr != s1->vertices.end(); vitr++) 
            if((*vitr == v1 && *(vitr - 1) == v2) || (*vitr == v2 && *(vitr - 1) == v1)) 
                return true;

    return false;
}

bool Mesh::connected(Surface *s1, Surface *s2) {
    for(auto &v : s1->parents()) 
        if(v->in(s2)) 
            return true;
    return false;
}

bool Mesh::connected(Body *b1, Body *b2) {
    for(auto &s : b1->parents()) 
        if(s->in(b2)) 
            return true;
    return false;
}

HRESULT Mesh_surfaceOutwardNormal(Surface *s, Body *b1, Body *b2, FVector3 &onorm) {
    if(b1 && b2) { 
        TF_Log(LOG_ERROR) << "Surface is twice-connected";
        return NULL;
    } 
    else if(b1) {
        onorm = s->getNormal();
    } 
    else if(b2) {
        onorm = -s->getNormal();
    } 
    else {
        onorm = s->getNormal();
    }
    return S_OK;
}

// Mesh editing

HRESULT Mesh::remove(Vertex *v) {
    return removeObj(v);
}

HRESULT Mesh::remove(Surface *s) {
    return removeObj(s);
}

HRESULT Mesh::remove(Body *b) {
    return removeObj(b);
}

HRESULT Mesh::insert(Vertex *toInsert, Vertex *v1, Vertex *v2) {
    Vertex *v;
    std::vector<Vertex*>::iterator vitr;

    // Find the common surface(s)
    int nidx;
    Vertex *vn;
    for(auto &s1 : v1->surfaces) {
        nidx = 0;
        for(vitr = s1->vertices.begin(); vitr != s1->vertices.end(); vitr++) {
            nidx++;
            if(nidx >= s1->vertices.size()) 
                nidx -= s1->vertices.size();
            vn = s1->vertices[nidx];

            if((*vitr == v1 && vn == v2) || (*vitr == v2 && vn == v1)) {
                s1->vertices.insert(vitr + 1 == s1->vertices.end() ? s1->vertices.begin() : vitr + 1, toInsert);
                toInsert->addChild(s1);
                break;
            }
        }
    }

    if(add(toInsert) != S_OK) 
        return E_FAIL;

    if(_solver) { 
        if(_solver->positionChanged() != S_OK)
            return E_FAIL;

        _solver->log(this, MeshLogEventType::Create, {v1->objId, v2->objId}, {v1->objType(), v2->objType()}, "insert");
    }

    return S_OK;
}

HRESULT Mesh::replace(Vertex *toInsert, Surface *toReplace) {
    // For every surface connected to the replaced surface
    //      Gather every vertex connected to the replaced surface
    //      Replace all vertices with the inserted vertex
    // Remove the replaced surface from the mesh
    // Add the inserted vertex to the mesh

    // Gather every contacting surface
    std::vector<Surface*> connectedSurfaces;
    for(auto &v : toReplace->vertices) 
        for(auto &s : v->surfaces) 
            if(s != toReplace && std::find(connectedSurfaces.begin(), connectedSurfaces.end(), s) != connectedSurfaces.end()) 
                connectedSurfaces.push_back(s);

    // Disconnect every vertex connected to the replaced surface
    unsigned int lab;
    std::vector<unsigned int> edgeLabels;
    std::vector<Vertex*> totalToRemove;
    for(auto &s : connectedSurfaces) {
        edgeLabels = s->contiguousEdgeLabels(toReplace);
        std::vector<Vertex*> toRemove;
        for(unsigned int i = 0; i < edgeLabels.size(); i++) {
            lab = edgeLabels[i];
            if(lab > 0) {
                if(lab > 1) {
                    TF_Log(LOG_ERROR) << "Replacement cannot occur over non-contiguous contacts";
                    return E_FAIL;
                }
                toRemove.push_back(s->vertices[i]);
            }
        }
        
        s->vertices.insert(std::find(s->vertices.begin(), s->vertices.end(), toRemove[0]), toInsert);
        toInsert->addChild(s);
        for(auto &v : toRemove) {
            s->removeParent(v);
            v->removeChild(s);
            totalToRemove.push_back(v);
        }
    }

    // Remove the replaced surface and its vertices
    if(removeObj(toReplace) != S_OK) 
        return E_FAIL;
    for(auto &v : totalToRemove) 
        if(removeObj(v) != S_OK) 
            return E_FAIL;

    // Add the inserted vertex
    if(add(toInsert) != S_OK) 
        return E_FAIL;

    if(_solver) { 
        if(_solver->positionChanged() != S_OK)
            return E_FAIL;

        _solver->log(this, MeshLogEventType::Create, {toInsert->objId, toReplace->objId}, {toInsert->objType(), toReplace->objType()}, "replace");
    }

    return S_OK;
}

Surface *Mesh::replace(SurfaceType *toInsert, Vertex *toReplace, std::vector<float> lenCfs) {
    std::vector<Vertex*> neighbors = toReplace->neighborVertices();
    if(lenCfs.size() != neighbors.size()) {
        TF_Log(LOG_ERROR) << "Length coefficients are inconsistent with connectivity";
        return NULL;
    } 

    for(auto &cf : lenCfs) 
        if(cf <= 0.f || cf >= 1.f) {
            TF_Log(LOG_ERROR) << "Length coefficients must be in (0, 1)";
            return NULL;
        }

    // Insert new vertices
    FVector3 pos0 = toReplace->getPosition();
    std::vector<Vertex*> insertedVertices;
    for(unsigned int i = 0; i < neighbors.size(); i++) {
        float cf = lenCfs[i];
        if(cf <= 0.f || cf >= 1.f) {
            TF_Log(LOG_ERROR) << "Length coefficients must be in (0, 1)";
            return NULL;
        }

        Vertex *v = neighbors[i];
        FVector3 pos1 = v->getPosition();
        FVector3 pos = pos0 + (pos1 - pos0) * cf;
        MeshParticleType *ptype = MeshParticleType_get();
        ParticleHandle *ph = (*ptype)(&pos);
        Vertex *vInserted = new Vertex(ph->id);
        if(insert(vInserted, toReplace, v) != S_OK) 
            return NULL;
        insertedVertices.push_back(vInserted);
    }

    // Disconnect replaced vertex from all surfaces
    std::vector<Surface*> toReplaceSurfaces(toReplace->surfaces.begin(), toReplace->surfaces.end());
    for(auto &s : toReplaceSurfaces) {
        s->removeParent(toReplace);
        toReplace->removeChild(s);
    }

    // Create new surface; its constructor should handle internal connections
    Surface *inserted = (*toInsert)(insertedVertices);

    // Remove replaced vertex from the mesh and add inserted surface to the mesh
    removeObj(toReplace);
    add(inserted);

    if(_solver) {
        _solver->positionChanged();

        _solver->log(this, MeshLogEventType::Create, {inserted->objId, toReplace->objId}, {inserted->objType(), toReplace->objType()}, "replace");
    }

    return inserted;
}

HRESULT Mesh::merge(Vertex *toKeep, Vertex *toRemove, const float &lenCf) {
    // Impose that vertices with shared surfaces must be adjacent
    auto sharedSurfaces = toKeep->sharedSurfaces(toRemove);
    if(sharedSurfaces.size() == 0) {
        TF_Log(LOG_ERROR) << "Vertices must be adjacent on shared surfaces";
        return E_FAIL;
    }
    auto s = sharedSurfaces[0];
    Vertex *vt;
    for(unsigned int i = 0; i < s->vertices.size(); i++) {
        auto v = s->vertices[i];
        vt = v == toKeep ? toRemove : (v == toRemove ? toKeep : NULL);

        if(vt) {
            if(vt != s->vertices[i + 1 >= s->vertices.size() ? i + 1 - s->vertices.size() : i + 1]) {
                TF_Log(LOG_ERROR) << "Vertices with shared surfaces must be adjacent";
                return E_FAIL;
            } 
            else 
                break;
        }
    }

    // Disconnect and remove vertex
    auto childrenToRemove = toRemove->children();
    for(auto &c : childrenToRemove) {
        if(toRemove->removeChild(c) != S_OK) 
            return E_FAIL;
        if(c->removeParent(toRemove) != S_OK) 
            return E_FAIL;
    }
    if(remove(toRemove) != S_OK) 
        return E_FAIL;
    
    // Set new position
    const FVector3 posToKeep = toKeep->getPosition();
    const FVector3 newPos = posToKeep + (toRemove->getPosition() - posToKeep) * lenCf;
    if(toKeep->setPosition(newPos) != S_OK) 
        return E_FAIL;

    if(_solver) { 
        if(_solver->positionChanged() != S_OK)
            return E_FAIL;

        _solver->log(this, MeshLogEventType::Create, {toKeep->objId, toRemove->objId}, {toKeep->objType(), toRemove->objType()}, "merge");
    }

    return S_OK;
}

HRESULT Mesh::merge(Surface *toKeep, Surface *toRemove, const std::vector<float> &lenCfs) {
    if(toKeep->vertices.size() != toRemove->vertices.size()) {
        TF_Log(LOG_ERROR) << "Surfaces must have the same number of vertices to merge";
        return E_FAIL;
    }

    // Find vertices that are not shared
    std::vector<Vertex*> toKeepExcl;
    for(auto &v : toKeep->vertices) 
        if(!v->in(toRemove)) 
            toKeepExcl.push_back(v);

    // Ensure sufficient length cofficients
    std::vector<float> _lenCfs = lenCfs;
    if(_lenCfs.size() < toKeepExcl.size()) {
        TF_Log(LOG_DEBUG) << "Insufficient provided length coefficients. Assuming 0.5";
        for(unsigned int i = _lenCfs.size(); i < toKeepExcl.size(); i++) 
            _lenCfs.push_back(0.5);
    }

    // Match vertex order of removed surface to kept surface by nearest distance
    std::vector<Vertex*> toRemoveOrdered;
    for(auto &kv : toKeepExcl) {
        Vertex *mv = NULL;
        FVector3 kp = kv->getPosition();
        float bestDist = 0.f;
        for(auto &rv : toRemove->vertices) {
            float dist = (rv->getPosition() - kp).length();
            if((!mv || dist < bestDist) && std::find(toRemoveOrdered.begin(), toRemoveOrdered.end(), rv) == toRemoveOrdered.end()) {
                bestDist = dist;
                mv = rv;
            }
        }
        if(!mv) {
            TF_Log(LOG_ERROR) << "Could not match surface vertices";
            return E_FAIL;
        }
        toRemoveOrdered.push_back(mv);
    }

    // Replace vertices in neighboring surfaces
    for(unsigned int i = 0; i < toKeepExcl.size(); i++) {
        Vertex *rv = toRemoveOrdered[i];
        Vertex *kv = toKeepExcl[i];
        std::vector<Surface*> rvSurfaces = rv->surfaces;
        for(auto &s : rvSurfaces) 
            if(s != toRemove) {
                if(std::find(s->vertices.begin(), s->vertices.end(), rv) == s->vertices.end()) {
                    TF_Log(LOG_ERROR) << "Something went wrong during surface merge";
                    return E_FAIL;
                }
                std::replace(s->vertices.begin(), s->vertices.end(), rv, kv);
                kv->surfaces.push_back(s);
            }
    }

    // Replace surface in child bodies
    for(auto &b : toRemove->getBodies()) {
        if(!toKeep->in(b)) {
            b->addParent(toKeep);
            toKeep->addChild(b);
        }
        b->removeParent(toRemove);
        toRemove->removeChild(b);
    }

    // Detach removed vertices
    for(auto &v : toRemoveOrdered) {
        v->surfaces.clear();
        toRemove->removeParent(v);
    }

    // Move kept vertices by length coefficients
    for(unsigned int i = 0; i < toKeepExcl.size(); i++) {
        Vertex *v = toKeepExcl[i];
        FVector3 posToKeep = v->getPosition();
        FVector3 newPos = posToKeep + (toRemoveOrdered[i]->getPosition() - posToKeep) * _lenCfs[i];
        if(v->setPosition(newPos) != S_OK) 
            return E_FAIL;
    }
    
    // Remove surface and vertices that are not shared
    if(remove(toRemove) != S_OK) 
        return E_FAIL;
    for(auto &v : toRemoveOrdered) 
        if(remove(v) != S_OK) 
            return E_FAIL;

    if(_solver) { 
        if(_solver->positionChanged() != S_OK)
            return E_FAIL;

        _solver->log(this, MeshLogEventType::Create, {toKeep->objId, toRemove->objId}, {toKeep->objType(), toRemove->objType()}, "merge");
    }

    return S_OK;
}

Surface *Mesh::extend(Surface *base, const unsigned int &vertIdxStart, const FVector3 &pos) {
    // Validate indices
    if(vertIdxStart >= base->vertices.size()) {
        TF_Log(LOG_ERROR) << "Invalid vertex indices (" << vertIdxStart << ", " << base->vertices.size() << ")";
        return NULL;
    }

    // Get base vertices
    Vertex *v0 = base->vertices[vertIdxStart];
    Vertex *v1 = base->vertices[vertIdxStart == base->vertices.size() - 1 ? 0 : vertIdxStart + 1];

    // Construct new vertex at specified position
    SurfaceType *stype = base->type();
    MeshParticleType *ptype = MeshParticleType_get();
    FVector3 _pos = pos;
    ParticleHandle *ph = (*ptype)(&_pos);
    Vertex *vert = new Vertex(ph->id);

    // Construct new surface, add new parts and return
    Surface *s = (*stype)({v0, v1, vert});
    add(s);

    if(_solver) {
        _solver->positionChanged();

        _solver->log(this, MeshLogEventType::Create, {base->objId, s->objId}, {base->objType(), s->objType()}, "extend");
    }

    return s;
}

Surface *Mesh::extrude(Surface *base, const unsigned int &vertIdxStart, const float &normLen) {
    // Validate indices
    if(vertIdxStart >= base->vertices.size()) {
        TF_Log(LOG_ERROR) << "Invalid vertex indices (" << vertIdxStart << ", " << base->vertices.size() << ")";
        return NULL;
    }

    // Get base vertices
    Vertex *v0 = base->vertices[vertIdxStart];
    Vertex *v1 = base->vertices[vertIdxStart == base->vertices.size() - 1 ? 0 : vertIdxStart + 1];

    // Construct new vertices
    FVector3 disp = base->normal * normLen;
    FVector3 pos2 = v0->getPosition() + disp;
    FVector3 pos3 = v1->getPosition() + disp;
    MeshParticleType *ptype = MeshParticleType_get();
    ParticleHandle *p2 = (*ptype)(&pos2);
    ParticleHandle *p3 = (*ptype)(&pos3);
    Vertex *v2 = new Vertex(p2->id);
    Vertex *v3 = new Vertex(p3->id);

    // Construct new surface, add new parts and return
    SurfaceType *stype = base->type();
    Surface *s = (*stype)({v0, v1, v2, v3});
    add(s);

    if(_solver) {
        _solver->positionChanged();

        _solver->log(this, MeshLogEventType::Create, {base->objId, s->objId}, {base->objType(), s->objType()}, "extrude");
    }

    return s;
}

Body *Mesh::extend(Surface *base, BodyType *btype, const FVector3 &pos) {
    // For every pair of vertices, construct a surface with a new vertex at the given position
    Vertex *vNew = new Vertex(pos);
    SurfaceType *stype = base->type();
    std::vector<Surface*> surfaces(1, base);
    for(unsigned int i = 0; i < base->vertices.size(); i++) {
        // Get base vertices
        Vertex *v0 = base->vertices[i];
        Vertex *v1 = base->vertices[i == base->vertices.size() - 1 ? 0 : i + 1];

        Surface *s = (*stype)({v0, v1, vNew});
        if(!s) 
            return NULL;
        surfaces.push_back(s);
    }

    // Construct a body from the surfaces
    Body *b = (*btype)(surfaces);
    if(!b) 
        return NULL;

    // Add new parts and return
    add(b);

    if(_solver) {
        _solver->positionChanged();

        _solver->log(this, MeshLogEventType::Create, {base->objId, b->objId}, {base->objType(), b->objType()}, "extend");
    }

    return b;
}

Body *Mesh::extrude(Surface *base, BodyType *btype, const float &normLen) {
    unsigned int i, j;
    FVector3 normal;

    // Only permit if the surface has an available slot
    base->refreshBodies();
    if(Mesh_surfaceOutwardNormal(base, base->b1, base->b2, normal) != S_OK) 
        return NULL;

    std::vector<Vertex*> newVertices(base->vertices.size(), 0);
    SurfaceType *stype = base->type();
    MeshParticleType *ptype = MeshParticleType_get();
    FVector3 disp = normal * normLen;

    for(i = 0; i < base->vertices.size(); i++) {
        FVector3 pos = base->vertices[i]->getPosition() + disp;
        ParticleHandle *ph = (*ptype)(&pos);
        newVertices[i] = new Vertex(ph->id);
    }

    std::vector<Surface*> newSurfaces;
    for(i = 0; i < base->vertices.size(); i++) {
        j = i + 1 >= base->vertices.size() ? i + 1 - base->vertices.size() : i + 1;
        Surface *s = (*stype)({
            base->vertices[i], 
            base->vertices[j], 
            newVertices[j], 
            newVertices[i]
        });
        if(!s) 
            return NULL;
        newSurfaces.push_back(s);
    }
    newSurfaces.push_back(base);
    newSurfaces.push_back((*stype)(newVertices));

    Body *b = (*btype)(newSurfaces);
    if(!b) 
        return NULL;
    add(b);

    if(_solver) {
        _solver->positionChanged();

        _solver->log(this, MeshLogEventType::Create, {base->objId, b->objId}, {base->objType(), b->objType()}, "extrude");
    }

    return b;
}

HRESULT Mesh::sew(Surface *s1, Surface *s2, const float &distCf) {
    // Verify that surfaces are in the mesh
    if(s1->mesh != this || s2->mesh != this) {
        TF_Log(LOG_ERROR) << "Surface not in this mesh";
        return E_FAIL;
    }

    if(Surface::sew(s1, s2, distCf) != S_OK) 
        return E_FAIL;

    if(_solver) 
        _solver->log(this, MeshLogEventType::Create, {s1->objId, s2->objId}, {s1->objType(), s2->objType()}, "sew");

    return S_OK;
}

HRESULT Mesh::sew(std::vector<Surface*> _surfaces, const float &distCf) {
    for(auto &si : _surfaces) 
        for(auto &sj : _surfaces) 
            if(si != sj && sew(si, sj, distCf) != S_OK) 
                return E_FAIL;
    return S_OK;
}
