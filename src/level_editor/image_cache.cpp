#include <cstddef>
#include <algorithm>
#include "image_cache.hpp"
#include "image_data.hpp"
#include "filesystem.hpp"
#include <core/helper.h>

using namespace Graal;
using namespace Graal::level_editor;

namespace {
  struct read_data {
    const char* data;
    std::size_t pos;
    std::size_t len;
  };

  cairo_status_t my_read_func(void* closure,
                              unsigned char* data,
                              unsigned int length) {
    read_data& state(*reinterpret_cast<read_data*>(closure));
    length = std::min(static_cast<std::size_t>(length), state.len - state.pos);
      
    const char* current_pos = state.data + state.pos;
    std::copy(current_pos, current_pos + length, data);
    state.pos += length;
    return CAIRO_STATUS_SUCCESS;
  }

  Cairo::RefPtr<Cairo::ImageSurface> load_image_file(const std::string& file_name) {
    std::string extension = file_name.substr(file_name.find_last_of(".") + 1);
    Graal::str_tolower(extension);

    // TODO: temporary solution, ideally figure out (and fix) why
    // Gdk::Pixbuf::create_from_file crashes on unsupported files
    if (extension != "gif" &&
        extension != "png" &&
        extension != "jpg" &&
        extension != "jpeg")
      throw std::runtime_error("Extension not supported");

    Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create_from_file(file_name);

    Cairo::Format format = Cairo::FORMAT_RGB24;
    if (pixbuf->get_has_alpha()) {
      format = Cairo::FORMAT_ARGB32;
    }

    Cairo::RefPtr<Cairo::ImageSurface> surface =
      Cairo::ImageSurface::create(format, pixbuf->get_width(), pixbuf->get_height());

    Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(surface);
    Gdk::Cairo::set_source_pixbuf(context, pixbuf, 0.0, 0.0);
    context->paint();

    return surface;
  }
}

image_cache::image_cache(filesystem& fs) : m_fs(fs) {
  load_internal_images();

  m_default_image = m_cache["internal/no_img.png"];
  m_cache[""] = m_npc_image = m_cache["internal/npc_default.png"];
}

image_cache::image_ptr& image_cache::get_image(const std::string& file_name) {
  image_ptr& image(m_cache[file_name]);

  if (image)
    return image;
  image = m_default_image; // writes default image into cache!

  boost::filesystem::path path;
  if (m_fs.get_path(file_name, path)) {
    // catch & ignore all exceptions
    try {
      image = load_image_file(path.string()); // Cairo::ImageSurface::create_from_png(path.string());
    } catch (std::exception&) {}
  }
  return image;
} 

void image_cache::clear_cache() {
  m_cache.clear();
  m_cache[""] = m_npc_image;
  load_internal_images();
  m_signal_cache_update.emit();
}

image_cache::signal_cache_update_type& image_cache::signal_cache_update() {
  return m_signal_cache_update;
}

void image_cache::load_internal_images() {
  const char** p = image_data::images;
  while (*p) {
    const char* name  = *(p++);
    const char* begin = *(p++);
    const char* end   = *(p++);

    read_data state = { begin, 0, end - begin};
    // do not handle cairo exceptions - if files do not load, something went
    // wrong during the build, so Cairo::logic_error is appropraite
    m_cache[name] = Cairo::ImageSurface::create_from_png(my_read_func, &state);
  }
}
