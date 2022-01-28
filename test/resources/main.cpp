/**
 * Open Space Program
 * Copyright © 2019-2021 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <osp/Resource/resources.h>

#include <gtest/gtest.h>

using namespace osp;

struct ImageData { int m_dummy{0}; };
struct TextureData { int m_dummy{0}; };
struct MeshData { int m_dummy{0}; };
struct ExtraData { int m_dummy{0}; };

Resources setup_basic()
{
    Resources res;

    // Size needed to fit all stock IDs in osp::restypes
    res.resize_types(gc_resTypeCount);

    res.data_register<ImageData>    (restypes::gc_image);

    res.data_register<TextureData>  (restypes::gc_texture);
    res.data_register<ExtraData>    (restypes::gc_texture);

    res.data_register<MeshData>     (restypes::gc_mesh);
    res.data_register<ExtraData>    (restypes::gc_mesh);

    return res;
}

// Test basic usage
TEST(Resources, Basic)
{
    Resources res = setup_basic();

    PkgId pkgA = res.pkg_create();

    // Add resources
    {
        ResId imageId = res.create(restypes::gc_image, pkgA, "Image0");

        EXPECT_EQ(res.data_try_get<ImageData>(restypes::gc_image, imageId), nullptr);
        auto &rImageData = res.data_add<ImageData>(restypes::gc_image, imageId, ImageData{42});
        EXPECT_EQ(rImageData.m_dummy, 42);

        auto *pImageData = res.data_try_get<ImageData>(restypes::gc_image, imageId);
        EXPECT_EQ(&rImageData, pImageData);
    }

    // Non-existent resources should be id_null
    EXPECT_EQ(res.find(restypes::gc_image, pkgA, "Does/Not/Exist"),
              lgrn::id_null<ResId>());

    // Get resources by name
    {
        ResId imageId = res.find(restypes::gc_image, pkgA, "Image0");
        EXPECT_NE(imageId, lgrn::id_null<ResId>());

        auto &rImageData = res.data_get<ImageData>(restypes::gc_image, imageId);
        EXPECT_EQ(rImageData.m_dummy, 42);
    }

    // Const access
    {
        Resources const& resConst = res;
        ResId imageId = resConst.find(restypes::gc_image, pkgA, "Image0");
        EXPECT_NE(imageId, lgrn::id_null<ResId>());

        auto const &rImageData = resConst.data_get<ImageData const>(restypes::gc_image, imageId);
        EXPECT_EQ(rImageData.m_dummy, 42);
    }

}

// Test ref counting and storage features
TEST(Resources, RefCounting)
{
    {
        Resources res = setup_basic();
        PkgId pkgA = res.pkg_create();

        ResId id = res.create(restypes::gc_image, pkgA, "Image0");
        ResIdStorage_t storage;
        EXPECT_FALSE(storage.has_value());
        res.store(restypes::gc_image, id, storage);
        EXPECT_TRUE(storage.has_value());
        res.release(restypes::gc_image, storage);
        EXPECT_FALSE(storage.has_value());
    }

    #ifdef NDEBUG
        GTEST_SKIP(); // following death tests use asserts
    #endif

    // Destruct ResIdStorage_t while it holds a value
    EXPECT_DEATH({
        Resources res = setup_basic();
        PkgId pkgA = res.pkg_create();

        ResId id = res.create(restypes::gc_image, pkgA, "Image0");
        {
            ResIdStorage_t storage;
            res.store(restypes::gc_image, id, storage);
        }
    }, "has_value\\(\\)");

    // Destruct Resources with non-zero reference counts
    EXPECT_DEATH({
        ResIdStorage_t storage;

        {
            Resources res = setup_basic();
            PkgId pkgA = res.pkg_create();
            ResId id = res.create(restypes::gc_image, pkgA, "Image0");

            res.store(restypes::gc_image, id, storage);
        }
    }, "only_zeros_remaining\\(0\\)");


}
