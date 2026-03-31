// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/common/graphics/texture.h"
#include "flutter/display_list/dl_builder.h"
#include "flutter/fml/thread.h"
#include "flutter/lib/ui/painting/display_list_texture_registry.h"
#include "flutter/lib/ui/painting/testing/mocks.h"
#include "flutter/testing/post_task_sync.h"
#include "flutter/testing/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "impeller/core/texture_descriptor.h"
#include "impeller/renderer/testing/mocks.h"

namespace flutter {
namespace testing {

// A fake Texture where GetTextureImage returns the image argument.
class FakeExternalTexture : public flutter::Texture {
 public:
  explicit FakeExternalTexture(int64_t id, sk_sp<DlImage> image = nullptr)
      : Texture(id), image_(std::move(image)) {}

  void Paint(PaintContext& context,
             const DlRect& bounds,
             bool freeze,
             const DlImageSampling sampling) override {
    if (context.canvas) {
      context.canvas->DrawRect(bounds, DlPaint());
    }
  }

  sk_sp<DlImage> GetTextureImage(PaintContext& context,
                                 const DlRect& bounds,
                                 bool freeze) override {
    return image_;
  }

  void OnGrContextCreated() override {}
  void OnGrContextDestroyed() override {}
  void MarkNewFrameAvailable() override {}
  void OnTextureUnregistered() override {}

 private:
  sk_sp<DlImage> image_;
};

TEST(DlImageTextureRegistry, ZeroCopy) {
  fml::Thread raster_thread("raster");
  auto task_runner = raster_thread.GetTaskRunner();
  const DlISize size = {100, 200};
  const int64_t texture_id = 42;

  impeller::TextureDescriptor desc;
  desc.size = {100, 200};
  auto mock_impeller_texture =
      std::make_shared<impeller::testing::MockTexture>(desc);

  auto mock_dl_image = sk_make_sp<MockDlImage>();
  ON_CALL(*mock_dl_image, impeller_texture)
      .WillByDefault(::testing::Return(mock_impeller_texture));

  std::unique_ptr<MockSnapshotDelegate> snapshot_delegate;
  fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate_weak_ptr;
  PostTaskSync(task_runner, [&]() {
    snapshot_delegate = std::make_unique<MockSnapshotDelegate>();

    auto registry = snapshot_delegate->GetMockTextureRegistry();
    auto fake_texture =
        std::make_shared<FakeExternalTexture>(texture_id, mock_dl_image);
    registry->RegisterTexture(fake_texture);

    ON_CALL(*snapshot_delegate, GetTextureRegistry())
        .WillByDefault(::testing::Return(registry));

    snapshot_delegate_weak_ptr = snapshot_delegate->GetWeakPtr();
  });

  auto image =
      DlImageTextureRegistry::Make(texture_id, size, /*freeze=*/true,
                                   snapshot_delegate_weak_ptr, task_runner);
  ASSERT_EQ(image->GetSize(), size);

  PostTaskSync(task_runner, [&]() {
    EXPECT_TRUE(image->impeller_texture());
    EXPECT_EQ(image->impeller_texture(), mock_impeller_texture);
    snapshot_delegate.reset();
  });
}

TEST(DlImageTextureRegistry, Fallback) {
  fml::Thread raster_thread("raster");
  auto task_runner = raster_thread.GetTaskRunner();
  const DlISize size = {100, 200};
  const int64_t texture_id = 42;

  impeller::TextureDescriptor desc;
  desc.size = {100, 200};
  auto fallback_impeller_texture =
      std::make_shared<impeller::testing::MockTexture>(desc);

  auto fallback_dl_image = sk_make_sp<MockDlImage>();
  ON_CALL(*fallback_dl_image, impeller_texture)
      .WillByDefault(::testing::Return(fallback_impeller_texture));

  std::unique_ptr<MockSnapshotDelegate> snapshot_delegate;
  fml::TaskRunnerAffineWeakPtr<SnapshotDelegate> snapshot_delegate_weak_ptr;
  PostTaskSync(task_runner, [&]() {
    snapshot_delegate = std::make_unique<MockSnapshotDelegate>();

    // Register a texture without a DlImage (GetTextureImage returns nullptr).
    auto registry = snapshot_delegate->GetMockTextureRegistry();
    auto fake_texture = std::make_shared<FakeExternalTexture>(texture_id);
    registry->RegisterTexture(fake_texture);

    ON_CALL(*snapshot_delegate, GetTextureRegistry())
        .WillByDefault(::testing::Return(registry));

    EXPECT_CALL(
        *snapshot_delegate,
        MakeRasterSnapshotSync(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(fallback_dl_image));

    snapshot_delegate_weak_ptr = snapshot_delegate->GetWeakPtr();
  });

  auto image =
      DlImageTextureRegistry::Make(texture_id, size, /*freeze=*/true,
                                   snapshot_delegate_weak_ptr, task_runner);
  ASSERT_EQ(image->GetSize(), size);

  PostTaskSync(task_runner, [&]() {
    EXPECT_TRUE(image->impeller_texture());
    EXPECT_EQ(image->impeller_texture(), fallback_impeller_texture);
    snapshot_delegate.reset();
  });
}

}  // namespace testing
}  // namespace flutter
